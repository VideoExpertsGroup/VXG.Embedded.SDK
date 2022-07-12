#pragma once

#include <utils/profile.h>

#include <atomic>
#include <future>

#include <agent/timeline.h>

namespace vxg {
namespace cloud {
namespace agent {
class synchronizer {
    vxg::logger::logger_ptr logger {
        vxg::logger::instance("timeline-synchronizer")};

public:
    enum class sync_request_status { PENDING, DONE, ERROR, CANCELED };

    struct config {
        //! @brief by event recording segment duration
        std::chrono::seconds record_by_event_upload_step {
            std::chrono::seconds(15)};
    };

    struct segmenter;
    using sync_status_report_cb =
        std::function<void(int progress,
                           sync_request_status status,
                           std::shared_ptr<segmenter> seg)>;

    struct segmenter : public cloud::period {
        cloud::time cur_seg_start;
        cloud::time cur_seg_stop;
        cloud::time last_processed_time;
        cloud::duration step;
        cloud::duration delay;
        //! Processing finished, doesn't mean upload of all processed chunks
        //! is finished
        std::atomic<bool> processed {false};
        //! Canceled
        std::atomic<bool> canceled {false};
        //! Upload of all processed chunks finished, no matter what was result
        //! of the chunk upload attempt.
        std::atomic<bool> finished {false};
        //! Realtime delay between chunks processing
        std::atomic<bool> realtime {true};
        std::string ticket;
        size_t chunks_planned {0};
        size_t chunks_done {0};
        size_t chunks_failed {0};
        sync_status_report_cb sync_status_cb;
        bool final_sync_status_reported;

        virtual ~segmenter() {}

        bool operator<(const segmenter& r) {
            return cur_seg_start < r.cur_seg_start;
        }

        bool intersects(const segmenter& r) {
            auto b1 = begin;
            auto b2 = r.begin;
            auto e1 = end != utils::time::null() ? end : utils::time::max();
            auto e2 = r.end != utils::time::null() ? r.end : utils::time::max();

            if (b1 <= e2 && e1 >= b2)
                return true;
            return false;
        }

        typedef std::shared_ptr<segmenter> ptr;
    };
    using segmenter_ptr = std::shared_ptr<segmenter>;

    struct sync_request {
        segmenter_ptr segmenter;
    };
    using sync_request_ptr = std::shared_ptr<sync_request>;

private:
    vxg::cloud::sync::timeline_ptr src_timeline_;
    vxg::cloud::sync::timeline_ptr dst_timeline_;
    transport::libwebsockets::http::ptr timer_provider_;
    vxg::cloud::transport::timed_cb_ptr loop_;
    std::map<segmenter_ptr, vxg::cloud::transport::timed_cb_ptr>
        scheduled_sync_starts_;
    // FIXME: Get rid of this uglyness
    std::vector<std::string> canceled_tickets;
    synchronizer::config config_;

    static bool __seg_compare(const segmenter_ptr& left,
                              const segmenter_ptr& right) {
        return left->begin < right->begin;
    }

    std::vector<segmenter_ptr> segmenters_;

    synchronizer(const synchronizer::config& config,
                 vxg::cloud::sync::timeline_ptr s,
                 vxg::cloud::sync::timeline_ptr d,
                 std::shared_ptr<transport::worker> timer_provider = nullptr)
        : config_ {config}, src_timeline_ {s}, dst_timeline_ {d} {
        if (timer_provider == nullptr)
            timer_provider_ =
                std::make_shared<transport::libwebsockets::http>();
    }

    void __report_timeline_synchronization_status(segmenter_ptr segment) {
        sync_request_status status;
        size_t progress = 0;

        // Progress is 0 if we are still planning the uploads
        if (segment->processed) {
            if (segment->chunks_planned == 0)
                progress = 100;
            else
                progress =
                    ((segment->chunks_done + segment->chunks_failed) * 100) /
                    segment->chunks_planned;
        } else
            progress = 0;

        // Check if synchro request for this segmenter's ticket was canceled
        // during the removing ticket from canceled_tickets.
        // FIXME: get rid of this uglyness
        canceled_tickets.erase(
            std::remove_if(canceled_tickets.begin(), canceled_tickets.end(),
                           [&](const std::string& t) {
                               bool canceled = (t == segment->ticket);
                               // Mark segmenter as canceled
                               if (canceled) {
                                   segment->canceled = true;
                                   logger->info(
                                       "Synchronization request {} for "
                                       "segmenter[{}] was canceled!",
                                       t, static_cast<void*>(segment.get()));
                               }
                               return canceled;
                           }),
            canceled_tickets.end());

        if (segment->final_sync_status_reported)
            return;

        if (segment->canceled)
            status = sync_request_status::CANCELED;
        else if (segment->finished) {
            status = segment->chunks_done == 0 ? sync_request_status::ERROR
                                               : sync_request_status::DONE;
            // // Prevent not late segments to report non-done
            // // state after the very last chunk was uploaded
            // segment->ticket.clear();
            segment->final_sync_status_reported = true;
            progress = 100;
        } else
            status = sync_request_status::PENDING;

        if (segment->sync_status_cb)
            segment->sync_status_cb(progress, status, segment);
    }

    void __adjust_delay(segmenter_ptr segment, timed_storage::item_ptr item) {
        auto now = utils::time::now();
        // Calculate delay for next iteration.
        if (segment->realtime) {
            auto extra_time_adj =
                ((now - segment->last_processed_time) - segment->delay);
            if (segment->step > extra_time_adj)
                segment->delay = segment->step - extra_time_adj;
            else
                segment->delay = std::chrono::seconds(0);

            // Adjust step value by the extra duration of the curent
            // chunk.
            // Chunk may be shorter than requested range or longer, we're
            // considering both cases with the sign of (item->end -
            // segment->cur_seg_stop)
            if (item->is_valid())
                segment->delay += item->end - segment->cur_seg_stop;
        } else
            segment->delay = std::chrono::seconds(0);

        segment->last_processed_time = now;
    }

    void __on_chunk_sync_done(segmenter_ptr s, bool ok) {
        s->chunks_done += ok;
        s->chunks_failed += !ok;
        s->finished = s->processed && (s->chunks_planned ==
                                       (s->chunks_done + s->chunks_failed));

        logger->info(
            "Sync request chunk upload done; segmenter[{}], success: {}, "
            "failed: {}, planned: {}, sync request processed: {}, sync request "
            "finished: {}",
            static_cast<void*>(s.get()), s->chunks_done, s->chunks_failed,
            s->chunks_planned, s->processed, s->finished);

        __report_timeline_synchronization_status(s);
    }

    bool __segment_step(segmenter_ptr segment) {
        using namespace std::chrono;

        if (!segment->processed && !segment->canceled) {
            logger->info(
                "Processing segmenter[{}] chunk {} - {} for segment {} - {}",
                static_cast<void*>(segment.get()),
                utils::time::to_iso_packed(segment->cur_seg_start),
                utils::time::to_iso_packed(segment->cur_seg_stop),
                utils::time::to_iso_packed(segment->begin),
                utils::time::to_iso_packed(segment->end));

            if (segment->end != utils::time::null() &&
                segment->cur_seg_stop > segment->end) {
                segment->cur_seg_stop = segment->end;
            }

            if (segment->cur_seg_start == segment->cur_seg_stop) {
                logger->warn("Empty chunk for segmenter[{}], skipping",
                             static_cast<void*>(segment.get()));
                segment->cur_seg_stop = segment->cur_seg_start + segment->step;
                return false;
            }

            auto items_list = src_timeline_->list(segment->cur_seg_start,
                                                  segment->cur_seg_stop);
            timed_storage::item_ptr item =
                std::make_shared<timed_storage::item>();
            if (!items_list.empty()) {
                item = items_list[0];
                segment->chunks_planned++;
                __adjust_delay(segment, item);

                logger->info(
                    "Records list requested for segmenter[{}] period {} - {}, "
                    "got {} items "
                    "for {} - {}, next step delay {:.3f}",
                    static_cast<void*>(segment.get()),
                    utils::time::to_iso_packed(segment->cur_seg_start),
                    utils::time::to_iso_packed(segment->cur_seg_stop),
                    items_list.size(),
                    utils::time::to_iso_packed(items_list.back()->begin),
                    utils::time::to_iso_packed(items_list.front()->end),
                    duration_cast<milliseconds>(segment->delay).count() /
                        (double)1000);

                logger->debug("Records list:");
                for (auto& i : items_list) {
                    logger->debug("{} - {}",
                                  utils::time::to_iso_packed(i->begin),
                                  utils::time::to_iso_packed(i->end));
                }
            } else {
                logger->warn(
                    "Records list requested for period {} - {}, nothing "
                    "returned, skipping requested period for segmenter[{}]",
                    utils::time::to_iso_packed(segment->cur_seg_start),
                    utils::time::to_iso_packed(segment->cur_seg_stop),
                    static_cast<void*>(segment.get()));
                __adjust_delay(segment, item);
            }

            // if the clip was exported we use clip's stop time as start time
            // for next chunk. if clip export failed we use calculated stop time
            // as next start time post time is always used as a step
            if (item->is_valid()) {
                if (item->end - item->begin > std::chrono::minutes(10)) {
                    logger->warn(
                        "Exported record is too big > 10 minutes, skipping");
                    segment->cur_seg_start = segment->cur_seg_stop;
                } else
                    segment->cur_seg_start = item->end;
            } else
                segment->cur_seg_start = segment->cur_seg_stop;
            segment->cur_seg_stop = segment->cur_seg_start + segment->step;

            // Final chunk, if we can't export this chunk then we have nothing
            // to do and have to stop so finalize upload for event anyway.
            if (segment->end != utils::time::null() &&
                segment->end <= segment->cur_seg_start) {
                segment->processed = true;

                if (segment->chunks_planned ==
                    segment->chunks_failed + segment->chunks_done) {
                    segment->finished = true;
                }

                logger->info(
                    "Final chunk for segmenter[{}] sync request {} - {}!",
                    static_cast<void*>(segment.get()),
                    utils::time::to_iso_packed(segment->begin),
                    utils::time::to_iso_packed(segment->end));
            }

            if (item->is_valid()) {
                if (src_timeline_->load(item) &&
                    dst_timeline_->store_async(
                        item,
                        std::bind(&synchronizer::__on_chunk_sync_done, this,
                                  segment, std::placeholders::_1),
                        [segment]() { return !!segment->canceled; })) {
                } else {
                    segment->chunks_failed++;

                    logger->warn(
                        "Failed to load and store storage item {} - {} for "
                        "segmenter[{}]",
                        utils::time::to_iso_packed(item->begin),
                        utils::time::to_iso_packed(item->end),
                        static_cast<void*>(segment.get()));
                }
            } else if (segment->processed && segment->chunks_planned == 0) {
                logger->warn(
                    "Failed to export any chunk for segmenter[{}] {} - {}",
                    static_cast<void*>(segment.get()),
                    utils::time::to_iso_packed(segment->begin),
                    utils::time::to_iso_packed(segment->end));
                // If we finished processing and no chunks were planned to
                // upload we must mark segmenter as finished
                segment->finished = true;
                // __report_timeline_synchronization_status(segment);
            } else {
                // Do nothing
            }
        } else {
            logger->info(
                "Sync request preprocessing for segmenter[{}] {} - {} {}!",
                static_cast<void*>(segment.get()),
                utils::time::to_iso_packed(segment->begin),
                utils::time::to_iso_packed(segment->end),
                segment->processed ? "FINISHED" : "CANCELED");
        }

        return (segment->processed || segment->canceled);
    }

    // We need to set chunk->stop in this function scheduled by
    // schedule_timed_cb() because all function called this way are serialized
    // and we don't need a lock
    bool __segmenter_finalize(segmenter_ptr chunk, cloud::time stop) {
        using namespace std::chrono;

        chunk->end = stop;

        return true;
    }

    //!
    //! @brief Check if current upload window of the segmenter is presented in
    //! the remote storage, to prevent the intersections
    //!
    //! @param s segmenter to check
    //! @return true Means the upload window wasn't changed and may be stored in
    //! the remote storage. We will run the _step() method to upload the current
    //! window in this case.
    //! @return false The upload window was adjusted due to intersection with
    //! the data in the remote storage, we need to rerun this method for new
    //! upload window to check it too.
    bool __check_clip_segment(segmenter_ptr s) {
        auto slices = dst_timeline_->slices(s->cur_seg_start, s->cur_seg_stop);
        // This is result value returned from this method
        bool upload_current_window = true;

        if (!slices.empty()) {
            logger->debug(
                "Destination storage has already data conflicting with "
                "segmenter[{}]!",
                static_cast<void*>(s.get()));
        }

        for (auto& slice : slices) {
            logger->debug("Slice on the Cloud {} - {}",
                          utils::time::to_iso_packed(slice.begin),
                          utils::time::to_iso_packed(slice.end));

            if (slice.intersects({s->cur_seg_start, s->cur_seg_stop})) {
                logger->debug(
                    "Slice on the Cloud {} - {} intersects with chunk {} - {}",
                    utils::time::to_iso_packed(slice.begin),
                    utils::time::to_iso_packed(slice.end),
                    utils::time::to_iso_packed(s->cur_seg_start),
                    utils::time::to_iso_packed(s->cur_seg_stop));

                // Only two possible operations we do here:
                // - move start forward to the slice's end
                // - move current seg's stop backward to the slice's start
                if (s->cur_seg_start >= slice.begin)
                    s->cur_seg_start = slice.end;
                else
                    s->cur_seg_stop = slice.begin;

                // Slice fully includes current segment, no need to upload
                if (s->cur_seg_stop <= s->cur_seg_start) {
                    logger->debug(
                        "Slice {} - {} fully includes segment chunk {} - {}, "
                        "no need to upload",
                        utils::time::to_iso_packed(slice.begin),
                        utils::time::to_iso_packed(slice.end),
                        utils::time::to_iso_packed(s->cur_seg_start),
                        utils::time::to_iso_packed(s->cur_seg_stop));
                    s->cur_seg_stop = s->cur_seg_start + s->step;
                    // We adjusted the upload window, must check it if it
                    // doesn't intersect if remote storage data, returning false
                    // here for this
                    upload_current_window = false;
                }

                if (s->end != utils::time::null() &&
                    s->cur_seg_start > s->end) {
                    logger->debug("Chunk already on the Cloud, finalize.");

                    s->processed = s->finished = true;
                    // We finished current segmenter, must stop and report this
                    // in the may stepping loop
                    upload_current_window = false;
                    break;
                }

                // Count intersection as successfully uploaded chunk to prevent
                // this segmenter to be treated as failed
                s->chunks_planned++;
                s->chunks_done++;
            }
        }

        return upload_current_window;
    }

    segmenter_ptr __get_current() {
        size_t i = 0;
        segmenter_ptr result = nullptr;

        if (segmenters_.size() > 0) {
            // Remove canceled segmenters
            segmenters_.erase(
                std::remove_if(
                    segmenters_.begin(), segmenters_.end(),
                    [&](const segmenter_ptr s) {
                        if (s->canceled)
                            logger->debug(
                                "Purging canceled segmenter[{}] {} - {}",
                                static_cast<void*>(s.get()),
                                utils::time::to_iso_packed(s->begin),
                                utils::time::to_iso_packed(s->end));
                        return !!(s->canceled);
                    }),
                segmenters_.end());

            // Sort by the start time
            std::sort(segmenters_.begin(), segmenters_.end(),
                      synchronizer::__seg_compare);
            // Find first non-processed segmenter
            for (i = 0; i < segmenters_.size() && segmenters_[i]->processed;
                 i++)
                ;

            // If no segmenter found we are all done with processing and can try
            // to clear all finished segmenters from the list, otherwise return
            // found segmenter
            if (i >= segmenters_.size()) {
                segmenters_.erase(
                    std::remove_if(
                        segmenters_.begin(), segmenters_.end(),
                        [&](const segmenter_ptr& s) {
                            if (s->finished) {
                                logger->trace(
                                    "Freeing segmenter[{}]: {} - {}, current "
                                    "window: "
                                    "{} - {}, p: {}, f: {}",
                                    static_cast<void*>(s.get()),
                                    utils::time::to_iso_packed(s->begin),
                                    utils::time::to_iso_packed(s->end),
                                    utils::time::to_iso_packed(
                                        s->cur_seg_start),
                                    utils::time::to_iso_packed(s->cur_seg_stop),
                                    s->processed, s->finished);
                            }
                            return !!s->finished;
                        }),
                    segmenters_.end());
            } else
                result = segmenters_[i];
        }

        return result;
    }

    void __async_loop() {
        cloud::duration delay = std::chrono::seconds(1);

        while (__adjust_segmenters_current_to_finished())
            ;

        if (!segmenters_.empty()) {
            auto s = __get_current();

            if (s != nullptr) {
                if (__check_clip_segment(s))
                    __segment_step(s);
                __report_timeline_synchronization_status(s);

                if (!s->processed)
                    delay = s->delay;
            }
        }

        loop_ = timer_provider_->schedule_timed_cb(
            std::bind(&synchronizer::__async_loop, this),
            std::chrono::duration_cast<std::chrono::milliseconds>(delay)
                .count());
    }

    void __segmenter_cancel_by_ticket(std::string ticket) {
        if (!ticket.empty()) {
            std::vector<segmenter_ptr>::iterator iter;
            auto pred_ticket_eq = [=](segmenter_ptr s) {
                bool cancel = (ticket == s->ticket) && !s->canceled;
                return cancel;
            };

            while ((iter = std::find_if(segmenters_.begin(), segmenters_.end(),
                                        pred_ticket_eq)) != segmenters_.end()) {
                auto s = *iter;
                s->canceled = true;
                s->finished = s->processed = true;

                // Canceling not yet started segmenter delayed sync
                if (scheduled_sync_starts_.count(s)) {
                    timer_provider_->cancel_timed_cb(scheduled_sync_starts_[s]);
                    scheduled_sync_starts_.erase(s);
                }

                logger->debug("Segmenter[{}]: {} - {}, ticket: {} canceled!",
                              static_cast<void*>(s.get()),
                              utils::time::to_iso_packed(s->begin),
                              utils::time::to_iso_packed(s->end), s->ticket);

                __report_timeline_synchronization_status(s);
            }

            // Save canceled ticket for the case when segmenter with that ticket
            // was already processed and removed from the list, but we already
            // uploading, there is no way to cancel HTTP ongoing upload and the
            // on_finished() callback will be called in the end, we have to send
            // synchronize_status with canceled status there.
            canceled_tickets.push_back(ticket);
        }
    }

    bool __segmenter_enqueue(segmenter_ptr seg) {
        scheduled_sync_starts_.erase(seg);
        segmenters_.push_back(seg);
        std::sort(segmenters_.begin(), segmenters_.end(),
                  synchronizer::__seg_compare);
        return true;
    }

    void __dump_segmenters(segmenter_ptr curr_seg) {
        size_t finished = 0, processed = 0;

        logger->trace("Trying to adjust intersections for segments:");

        for (size_t i = 0; i < segmenters_.size(); i++) {
            finished += segmenters_[i]->finished;
            processed += segmenters_[i]->processed;

            logger->trace(
                "{}segmenter[{}]: {} - {}, upload window: {} - {}, p: {}, f: "
                "{}",
                curr_seg == segmenters_[i] ? "Current " : "",
                static_cast<void*>(segmenters_[i].get()),
                utils::time::to_iso_packed(segmenters_[i]->begin),
                utils::time::to_iso_packed(segmenters_[i]->end),
                utils::time::to_iso_packed(segmenters_[i]->cur_seg_start),
                utils::time::to_iso_packed(segmenters_[i]->cur_seg_stop),
                segmenters_[i]->processed, segmenters_[i]->finished);
        }

        logger->debug("Processing {} segmenters; p: {}, f: {}, new: {}",
                      segmenters_.size(), processed, finished,
                      segmenters_.size() - processed);

        assert(finished <= processed);
    }

    bool __adjust_segmenters_current_to_finished() {
        // Current here is the one we are chunking rn
        auto s = __get_current();

        if (s) {
            __dump_segmenters(s);

            // Shift all non finished/canceled intersecting segmenters upload
            // time by upload time of the finished segmenters
            for (size_t i = 0; i < segmenters_.size(); i++) {
                if (s != segmenters_[i] && segmenters_[i]->processed &&
                    s->intersects(*segmenters_[i]) &&
                    s->cur_seg_start < segmenters_[i]->cur_seg_start) {
                    s->cur_seg_start = segmenters_[i]->cur_seg_start;
                    s->cur_seg_stop = s->cur_seg_start + s->step;

                    logger->debug(
                        "Segmenter [{}]: {} - {} processing window adjusted to "
                        "{} - {}",
                        static_cast<void*>(s.get()),
                        utils::time::to_iso_packed(s->begin),
                        utils::time::to_iso_packed(s->end),
                        utils::time::to_iso_packed(s->cur_seg_start),
                        utils::time::to_iso_packed(s->cur_seg_stop));
                    // Signalling that requested data was actually uploaded
                    s->chunks_planned++;
                    s->chunks_done++;
                    // Current segmenter's processing window was adjusted
                    // outside the borders, it means current segmenter's data is
                    // already processed by segmenters_[i] and current segmenter
                    // can be skipped.
                    if (s->cur_seg_start >= s->end) {
                        logger->debug(
                            "Segmenter's [{}] data was already fully uploaded, "
                            "finalizing.",
                            static_cast<void*>(s.get()));
                        s->processed = s->finished = true;
                        __report_timeline_synchronization_status(s);
                        // return true means we are updated the segments and
                        // caller should call us again
                        return true;
                    }
                }
            }
        }
        return false;
    }

    bool __schedule_sync(segmenter_ptr seg) {
        using namespace std::chrono;

        logger->debug(
            "Schedule processing of segmenter[{}], sync request for {} - {}",
            static_cast<void*>(seg.get()),
            utils::time::to_iso_packed(seg->begin),
            utils::time::to_iso_packed(seg->end));

        scheduled_sync_starts_[seg] = timer_provider_->schedule_timed_cb(
            std::bind(&synchronizer::__segmenter_enqueue, this, seg),
            std::chrono::duration_cast<std::chrono::milliseconds>(seg->delay)
                .count());

        return true;
    }

    void __finalize_sync(segmenter_ptr seg, cloud::time end) {
        logger->debug(
            "Finalize sync request for segmenter[{}] start: {} with end: {}",
            static_cast<void*>(seg.get()),
            utils::time::to_iso_packed(seg->begin),
            utils::time::to_iso_packed(end));

        timer_provider_->schedule_timed_cb(
            std::bind(&synchronizer::__segmenter_finalize, this, seg, end), 0);
    }

    void __cancel_sync(const std::string& ticket) {
        logger->debug("Canceling sync request with ticket '{}'", ticket);

        timer_provider_->schedule_timed_cb(
            std::bind(&synchronizer::__segmenter_cancel_by_ticket, this,
                      ticket),
            0);
    }

    void __purge_segmenters() {
        segmenters_.erase(
            std::remove_if(
                segmenters_.begin(), segmenters_.end(),
                [&](const segmenter_ptr& s) {
                    if (!s->finished || !s->processed)
                        logger->trace(
                            "Purging not finished segmenter[{}]: {} - {}, "
                            "current "
                            "window: {} - {}, p: {}, f: {}",
                            static_cast<void*>(s.get()),
                            utils::time::to_iso_packed(s->begin),
                            utils::time::to_iso_packed(s->end),
                            utils::time::to_iso_packed(s->cur_seg_start),
                            utils::time::to_iso_packed(s->cur_seg_stop),
                            s->processed, s->finished);
                    return true;
                }),
            segmenters_.end());
    }

public:
    typedef std::shared_ptr<synchronizer> ptr;

    static ptr create(const synchronizer::config& c,
                      vxg::cloud::sync::timeline_ptr s,
                      vxg::cloud::sync::timeline_ptr d) {
        struct make_shared_enabler : public synchronizer {
            make_shared_enabler(const synchronizer::config& c,
                                vxg::cloud::sync::timeline_ptr s,
                                vxg::cloud::sync::timeline_ptr d)
                : synchronizer(c, s, d) {}
        };

        if (s == nullptr || d == nullptr)
            return nullptr;

        return std::make_shared<make_shared_enabler>(c, s, d);
    }

    bool start() {
        if (!src_timeline_->init()) {
            logger->error("Unable to initialize source timeline storage.");
            return false;
        }

        if (!dst_timeline_->init()) {
            logger->error("Unable to initialize destination timeline storage.");
            return false;
        }

        timer_provider_->start();

        loop_ = timer_provider_->schedule_timed_cb(
            std::bind(&synchronizer::__async_loop, this), 1);
        return !!loop_;
    }

    void stop() {
        for (auto& t : scheduled_sync_starts_)
            timer_provider_->cancel_timed_cb(t.second);
        scheduled_sync_starts_.clear();

        if (loop_)
            timer_provider_->cancel_timed_cb(loop_);

        if (segmenters_.size()) {
            std::mutex lock;
            std::unique_lock<std::mutex> ulock(lock);
            std::condition_variable barrier;
            auto t = timer_provider_->schedule_timed_cb(
                [&]() {
                    {
                        std::lock_guard<std::mutex> l(lock);
                        __purge_segmenters();
                    }
                    barrier.notify_one();
                },
                0);
            barrier.wait(ulock, [&]() { return !segmenters_.size(); });
        }

        src_timeline_->finit();
        dst_timeline_->finit();
    }

    sync_request_ptr sync(
        cloud::time begin,
        cloud::time end = utils::time::null(),
        sync_status_report_cb status_report_cb = nullptr,
        std::string upload_token = "",
        cloud::duration delay = std::chrono::microseconds(0)) {
        segmenter_ptr segmenter = std::make_shared<synchronizer::segmenter>();

        segmenter->sync_status_cb = status_report_cb;
        segmenter->begin = begin;
        segmenter->end = end;
        segmenter->cur_seg_start = begin;
        segmenter->step = config_.record_by_event_upload_step;
        segmenter->cur_seg_stop =
            std::min(begin + segmenter->step,
                     end != utils::time::null() ? end : utils::time::max());
        segmenter->processed = false;
        segmenter->ticket = upload_token;
        segmenter->realtime = (end == utils::time::null());
        segmenter->delay = delay;
        segmenter->last_processed_time = utils::time::now();

        logger->debug(
            "Sync request: segmenter[{}]: {} - {}, token: {}, delay: {}",
            static_cast<void*>(segmenter.get()),
            utils::time::to_iso_packed(segmenter->begin),
            utils::time::to_iso_packed(segmenter->end), upload_token,
            segmenter->delay.count());

        if (!__schedule_sync(segmenter))
            return nullptr;

        auto result = std::make_shared<sync_request>();
        result->segmenter = segmenter;

        return result;
    }

    void sync_finalize(sync_request_ptr req, cloud::time end) {
        __finalize_sync(req->segmenter, end);
    }

    void sync_cancel(const std::string& ticket) { __cancel_sync(ticket); }
};
using synchronizer_ptr = std::shared_ptr<synchronizer>;
}  // namespace agent
}  // namespace cloud
}  // namespace vxg