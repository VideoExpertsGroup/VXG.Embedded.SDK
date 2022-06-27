#pragma once

#include <agent-proto/proto.h>
#include <agent/manager.h>
#include <agent/timeline.h>
#include <utils/logging.h>

#include <atomic>

namespace vxg {
namespace cloud {
namespace agent {
class segmented_uploader {
    vxg::logger::logger_ptr logger {
        vxg::logger::instance("segmented-uploader")};

public:
    struct segmenter : public cloud::period {
        cloud::time cur_seg_start;
        cloud::time cur_seg_stop;
        cloud::time last_processed_time;
        cloud::duration pre;
        cloud::duration post;
        cloud::duration step;
        cloud::duration delay;
        //! Processing finished, doesn't mean upload of all processed chunks
        //! is finished
        atomic_bool processed {false};
        //! Canceled
        atomic_bool canceled {false};
        //! Upload of all processed chunks finished, no matter what was result
        //! of the chunk upload attempt.
        atomic_bool finished {false};
        //! Realtime delay between chunks processing
        atomic_bool realtime {true};
        agent::media::stream::ptr seg_provider {nullptr};
        proto::event_object trigger;
        std::string ticket;
        size_t uploads_planned {0};
        size_t uploads_done {0};
        size_t uploads_failed {0};

        ~segmenter() {}

        bool operator<(const segmenter& r) {
            return cur_seg_start < r.cur_seg_start;
        }

        bool intersects(const segmenter& r) {
            auto b1 = begin - pre;
            auto b2 = r.begin - r.pre;
            auto e1 =
                end != utils::time::null() ? end + step : utils::time::max();
            auto e2 = r.end != utils::time::null() ? r.end + r.step
                                                   : utils::time::max();

            if (b1 <= e2 && e1 >= b2)
                return true;
            return false;
        }

        typedef std::shared_ptr<segmenter> ptr;
    };
    using segmenter_ptr = segmenter::ptr;

private:
    timeline<cloud_storage> cloud_timeline_;
    proto::access_token access_token_;
    transport::libwebsockets::http::ptr http_;
    std::atomic<bool> need_stop_http_ {false};
    vxg::cloud::transport::timed_cb_ptr loop_;
    using upload_finished_cb = std::function<void()>;
    using direct_upload_cb = std::function<bool(proto::get_direct_upload_url)>;
    direct_upload_cb direct_upload_cb_;
    using send_command_cb = std::function<bool(const json& msg)>;
    send_command_cb send_command_cb_ {nullptr};
    std::vector<std::string> canceled_tickets;

    static bool __seg_compare(const segmenter_ptr& left,
                              const segmenter_ptr& right) {
        return left->begin < right->begin;
    }

    std::vector<segmenter_ptr> segmenters_;
    cloud::time last_upload_stop_time_;
    std::map<segmenter_ptr, vxg::cloud::transport::timed_cb_ptr>
        scheduled_sync_starts_;

protected:
    segmented_uploader(const proto::access_token& access_token,
                       transport::libwebsockets::http::ptr http,
                       direct_upload_cb direct_upload_cb,
                       send_command_cb send_command_cb)
        : access_token_ {access_token},
          http_ {http},
          cloud_timeline_ {access_token, http},
          direct_upload_cb_ {direct_upload_cb},
          send_command_cb_ {send_command_cb} {
        if (http_ == nullptr)
            http_ = std::make_shared<transport::libwebsockets::http>();

        // Use proxy if access token contains it
        if (!__is_unset(access_token.proxy.socks5))
            http_->set_proxy(access_token.proxy.socks5);
    }

    ~segmented_uploader() { stop(); }

    void __send_timeline_synchronization_status(size_t progress,
                                                segmenter_ptr segment) {
        proto::command::cam_memorycard_synchronize_status status;
        status.cam_id = access_token_.camid;
        status.progress = std::min((size_t)100, progress);
        status.request_id = segment->ticket;

        // If empty its not memorycard sync request, no need to report status
        if (segment->ticket.empty())
            return;

        // Check if synchro request for this segmenter's ticket was canceled
        // during the removing ticket from canceled_tickets.
        canceled_tickets.erase(
            std::remove_if(
                canceled_tickets.begin(), canceled_tickets.end(),
                [&](const std::string& t) {
                    bool canceled = (t == segment->ticket);
                    // Mark segmenter as canceled
                    if (canceled) {
                        segment->canceled = true;
                        logger->info("Synchronization request {} was canceled!",
                                     t);
                    }
                    return canceled;
                }),
            canceled_tickets.end());

        if (segment->canceled)
            status.status = proto::command::MCSS_CANCELED;
        else if (segment->processed &&
                 segment->uploads_planned ==
                     (segment->uploads_done + segment->uploads_failed)) {
            status.status = segment->uploads_done == 0
                                ? proto::command::MCSS_ERROR
                                : proto::command::MCSS_DONE;
            // Prevent not late segments to report non-done
            // state after the very last chunk was uploaded
            segment->ticket.clear();
            status.progress = 100;
        } else
            status.status = proto::command::MCSS_PENDING;

        if (send_command_cb_)
            send_command_cb_(json(status));
    }

    void __handle_timeline_synchronization_status(
        proto::get_direct_upload_url& getUploadUrl,
        segmenter_ptr segment,
        vxg::cloud::time latest_time) {
        // If we have ticket we need to report progress for timeline
        // synchronization API.
        // Mark segment direct upload request by the memorycard sync
        // requestid for canceling if required in future.
        // If it's not memorycard sync request the ticket is empty.
        getUploadUrl.memorycard_sync_ticket = segment->ticket;

        getUploadUrl.is_canceled = [segment]() { return !!segment->canceled; };

        getUploadUrl.on_finished = [this, segment, latest_time](bool ok) {
            segment->uploads_done += ok;
            segment->uploads_failed += !ok;
            segment->finished =
                segment->processed &&
                (segment->uploads_planned ==
                 (segment->uploads_done + segment->uploads_failed));
            // Last chunk was already uploaded and we are the late
            // chunk, don't bother the cloud
            if (segment->ticket.empty())
                return;

            // Progress is 0 if we are still planning the uploads
            size_t progress =
                segment->processed
                    ? ((segment->uploads_done + segment->uploads_failed) *
                       100) /
                          segment->uploads_planned
                    : 0;

            __send_timeline_synchronization_status(progress, segment);
        };
    }

    bool __segment_step(segmenter_ptr segment) {
        using namespace std::chrono;

        if (!segment->processed && !segment->canceled) {
            logger->info(
                "Processing upload recorded chunk {} - {} for segment {} - {}",
                utils::time::to_iso(segment->cur_seg_start),
                utils::time::to_iso(segment->cur_seg_stop),
                utils::time::to_iso(segment->begin),
                utils::time::to_iso(segment->end));

            if (!segment->is_open() &&
                segment->cur_seg_stop > segment->end + segment->post) {
                segment->cur_seg_stop = segment->end + segment->post;
            }

            if (!segment->is_open() &&
                segment->cur_seg_start >= segment->end + segment->post) {
                logger->warn(
                    "Upload window is outside of the segmenter's borders, "
                    "finalizing");
                segment->processed = true;
                if (segment->uploads_planned == 0) {
                    segment->finished = true;
                    // We failed to process all chunks in segment
                    __send_timeline_synchronization_status(0, segment);
                }
                return false;
            }

            if (segment->cur_seg_start == segment->cur_seg_stop) {
                logger->warn("Empty chunk, skipping");
                segment->cur_seg_stop = segment->cur_seg_start + segment->step;
                return false;
            }

            // Get records, if records presented we pick one and schedule its
            // upload
            auto clips_list = segment->seg_provider->record_get_list(
                segment->cur_seg_start, segment->cur_seg_stop, true);
            proto::video_clip_info clip;

            if (!clips_list.empty())
                clip = clips_list[0];

            // Calculate delay for next iteration.
            if (segment->realtime) {
                //
                auto now = utils::time::now();
                auto real_delay = (now - segment->last_processed_time);

                if (real_delay > std::chrono::seconds(100)) {
                    logger->critical(
                        "Segmenter was delayed for too long {}ms! now {}, "
                        "last_processing_time {}",
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            real_delay)
                            .count(),
                        utils::time::to_iso_packed(now),
                        utils::time::to_iso_packed(
                            segment->last_processed_time));
                    // TODO: This also means we have not enough upload
                    // bandwidth, we need to inform user about this fact
                    // somehow, probably by issuing extra qos-report.
                    // Also we need to do something with long list of old
                    // segmenters?
                }

                auto extra_time_adj =
                    ((utils::time::now() - segment->last_processed_time) -
                     segment->delay);
                if (segment->step > extra_time_adj)
                    segment->delay = segment->step - extra_time_adj;
                else
                    segment->delay = std::chrono::seconds(0);

                // Adjust step value by the extra duration of the curent
                // segment.
                if (segment->cur_seg_stop < clip.tp_stop)
                    segment->delay += clip.tp_stop - segment->cur_seg_stop;
            } else
                segment->delay = std::chrono::seconds(0);

            segment->last_processed_time = utils::time::now();

            logger->info(
                "Record requested {} - {}, got {} - {} next step delay {:.3f}",
                utils::time::to_iso(segment->cur_seg_start),
                utils::time::to_iso(segment->cur_seg_stop),
                utils::time::to_iso(clip.tp_start),
                utils::time::to_iso(clip.tp_stop),
                duration_cast<milliseconds>(segment->delay).count() /
                    (double)1000);

            if (segment->delay > std::chrono::seconds(100)) {
                logger->critical("delay is too big");
                logger->flush();
            }

            if (segment->delay < std::chrono::seconds(0)) {
                logger->critical("delay is negative");
                logger->flush();
            }

            // if the clip was exported we use clip's stop time as start time
            // for next chunk. if clip export failed we use calculated stop time
            // as next start time post time is always used as a step
            if (clip.tp_stop != utils::time::null() &&
                clip.tp_start != utils::time::null()) {
                if (clip.tp_stop - clip.tp_start > std::chrono::minutes(10)) {
                    logger->warn(
                        "Exported record is too big > 10 minutes, skipping");
                    segment->cur_seg_start = segment->cur_seg_stop;
                } else
                    segment->cur_seg_start = clip.tp_stop;
            } else
                segment->cur_seg_start = segment->cur_seg_stop;
            segment->cur_seg_stop = segment->cur_seg_start + segment->step;

            // Final chunk, if we can't export this chunk then we have nothing
            // to do and have to stop so finalize upload for event anyway.
            if (segment->end != utils::time::null() &&
                segment->end + segment->post <= segment->cur_seg_start) {
                segment->processed = true;
                logger->info(
                    "Final chunk for segment {} - {}, pre {} post {}!",
                    utils::time::to_iso2(segment->begin),
                    utils::time::to_iso2(segment->end),
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        segment->pre)
                        .count(),
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        segment->post)
                        .count());
            }

            if (clip.tp_stop != utils::time::null() &&
                clip.tp_start != utils::time::null()) {
                proto::get_direct_upload_url getUploadUrl;

                getUploadUrl.category = proto::UC_RECORD;
                getUploadUrl.type = proto::MT_MP4;
                getUploadUrl.file_time =
                    utils::time::to_iso_packed(clip.tp_start);
                getUploadUrl.duration =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        clip.tp_stop - clip.tp_start)
                        .count();
                getUploadUrl.duration_us =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        clip.tp_stop - clip.tp_start);
                getUploadUrl.size = clip.data.size();
                getUploadUrl.stream_id = segment->seg_provider->cloud_name();

                segment->uploads_planned++;

                // If current upload is timeline synchronization request this
                // method adds progress report callback
                __handle_timeline_synchronization_status(getUploadUrl, segment,
                                                         clip.tp_stop);

                if (direct_upload_cb_)
                    direct_upload_cb_(getUploadUrl);
            } else if (segment->processed && segment->uploads_planned == 0) {
                logger->warn("Failed to export record chunk for {} - {}",
                             utils::time::to_iso(segment->begin),
                             utils::time::to_iso(segment->end));
                // If we finished processing and no chunks were planned to
                // upload we must mark segmenter as finished
                segment->finished = true;
                // We failed to process all chunks in segment
                __send_timeline_synchronization_status(0, segment);
            }
        } else {
            logger->info("Segmented upload preprocessing for {} - {} {}!",
                         utils::time::to_iso(segment->begin),
                         utils::time::to_iso(segment->end),
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

    bool __check_clip_segment(segmenter_ptr s) {
        auto slices = cloud_timeline_.slices(s->cur_seg_start, s->cur_seg_stop);
        // This is result value returned from this method
        bool upload_current_window = true;

        if (!slices.empty()) {
            logger->debug("Cloud has already uploaded data!");
        }

        for (auto& slice : slices) {
            logger->debug("Slice on the Cloud {} - {}",
                          utils::time::to_iso(slice.begin),
                          utils::time::to_iso(slice.end));

            if (slice.intersects({s->cur_seg_start, s->cur_seg_stop})) {
                logger->debug(
                    "Slice on the Cloud {} - {} intersects with chunk {} - {}",
                    utils::time::to_iso(slice.begin),
                    utils::time::to_iso(slice.end),
                    utils::time::to_iso(s->cur_seg_start),
                    utils::time::to_iso(s->cur_seg_stop));

                if (s->cur_seg_start >= slice.begin)
                    s->cur_seg_start = slice.end;
                else
                    s->cur_seg_stop = slice.begin;

                // Slice fully includes current segment, no need to upload
                if (s->cur_seg_stop <= s->cur_seg_start) {
                    logger->debug(
                        "Slice {} - {} fully includes segment chunk {} - {}, "
                        "no need to upload",
                        utils::time::to_iso(slice.begin),
                        utils::time::to_iso(slice.end),
                        utils::time::to_iso(s->cur_seg_start),
                        utils::time::to_iso(s->cur_seg_stop));
                    s->cur_seg_stop = s->cur_seg_start + s->step;
                    // We adjusted the upload window, must check it if it
                    // doesn't intersect if remote storage data, returning false
                    // here for this
                    upload_current_window = false;
                }

                if (s->cur_seg_start > s->end + s->post) {
                    logger->debug("Segment already on the Cloud, finalize.");

                    s->processed = s->finished = true;
                    if (s->uploads_planned == 0)
                        __send_timeline_synchronization_status(100, s);
                    // We finished current segmenter, must stop and report this
                    // in the may stepping loop
                    upload_current_window = false;
                    break;
                }
            }
        }

        return upload_current_window;
    }

    void __purge_segmenters(bool finished, bool processed, size_t num) {
        size_t to_free = std::min(num, segmenters_.size());
        auto need_free_pred = [&](segmenter_ptr s) {
            return ((finished && s->finished) ||
                    (!finished && (processed && s->processed)) ||
                    (!finished && !processed)) &&
                   to_free;
        };

        if (to_free)
            segmenters_.erase(
                std::remove_if(
                    segmenters_.begin(), segmenters_.end(),
                    [&](const segmenter_ptr& s) {
                        bool need_remove = need_free_pred(s);
                        if (need_remove) {
                            logger->trace(
                                "Freeing {} segmenter[{}]: [{}/{}] {} - "
                                "{}, current window: {} - {}, p: {}, f: {}",
                                s->finished
                                    ? "finished"
                                    : (s->processed
                                           ? "processed"
                                           : "not finished and not processed"),
                                static_cast<void*>(s.get()), num - to_free + 1,
                                num, utils::time::to_iso_packed(s->begin),
                                utils::time::to_iso_packed(s->end),
                                utils::time::to_iso_packed(s->cur_seg_start),
                                utils::time::to_iso_packed(s->cur_seg_stop),
                                s->processed, s->finished);
                        }
                        to_free -= need_remove;

                        return need_remove;
                    }),
                segmenters_.end());
    }

    segmenter_ptr __get_current() {
        size_t i = 0;
        segmenter_ptr result = nullptr;

        if (segmenters_.size() > 0) {
            // Remove canceled segmenters
            segmenters_.erase(
                std::remove_if(segmenters_.begin(), segmenters_.end(),
                               [&](const segmenter_ptr s) {
                                   if (s->canceled)
                                       logger->debug(
                                           "Purging canceled segmenter {} - {}",
                                           utils::time::to_iso(s->begin),
                                           utils::time::to_iso(s->end));
                                   return !!(s->canceled);
                               }),
                segmenters_.end());

            // Sort by the start time
            std::sort(segmenters_.begin(), segmenters_.end(),
                      segmented_uploader::__seg_compare);
            // Find first non-processed segmenter, count all finished
            size_t num_finished = 0;
            for (i = 0; i < segmenters_.size() && segmenters_[i]->processed;
                 num_finished += segmenters_[i]->finished, i++)
                ;

            // If no segmenter found we are all done with processing and can try
            // to clear all finished segmenters from the list, otherwise return
            // found segmenter
            // If segmenter was found we will try to free half of the finished
            // segmenters, we don't delete the last finished segmenters because
            // they may have info about already uploaded data of the currently
            // processing segmenter
            if (i < segmenters_.size())
                result = segmenters_[i];

            __purge_segmenters(true, true, num_finished / 2);
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

                if (!s->processed)
                    delay = s->delay;
            }
        }

        loop_ = http_->schedule_timed_cb(
            std::bind(&segmented_uploader::__async_loop, this),
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
                    http_->cancel_timed_cb(scheduled_sync_starts_[s]);
                    scheduled_sync_starts_.erase(s);
                }

                logger->debug("Segmenter: {} - {}, ticket: {} canceled!",
                              utils::time::to_iso(s->begin),
                              utils::time::to_iso(s->end), s->ticket);

                __send_timeline_synchronization_status(100, s);
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
                  segmented_uploader::__seg_compare);
        return true;
    }

    void __dump_segmenters(segmenter_ptr curr_seg) {
        size_t finished = 0, processed = 0;

        logger->trace("Trying to adjust intersections for segments:");

        for (size_t i = 0; i < segmenters_.size(); i++) {
            finished += segmenters_[i]->finished;
            processed += segmenters_[i]->processed;

            // logger->trace(
            //     "{}segment: {} - {}, upload window: {} - {}, p: "
            //     "{}, f: {}",
            //     curr_seg == segmenters_[i] ? "Current " : "",
            //     utils::time::to_iso_packed(segmenters_[i]->begin -
            //                                segmenters_[i]->pre),
            //     utils::time::to_iso_packed(segmenters_[i]->end +
            //                                segmenters_[i]->post),
            //     utils::time::to_iso_packed(segmenters_[i]->cur_seg_start),
            //     utils::time::to_iso_packed(segmenters_[i]->cur_seg_stop),
            //     segmenters_[i]->processed, segmenters_[i]->finished);
        }

        logger->debug("Processing {} segmenters; p: {}, f: {}, new: {}",
                      segmenters_.size(), processed, finished,
                      segmenters_.size() - finished - processed);
        logger->flush();
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
                        "Segmenter {} - {} processing window adjusted to {} - "
                        "{}",
                        utils::time::to_iso_packed(s->begin - s->pre),
                        utils::time::to_iso_packed(s->end + s->post),
                        utils::time::to_iso_packed(s->cur_seg_start),
                        utils::time::to_iso_packed(s->cur_seg_stop));

                    // Current segmenter's processing window was adjusted
                    // outside the borders, it means current segmenter's data is
                    // already processed by segmenters_[i] and current segmenter
                    // can be skipped.
                    if (!s->is_open() && s->cur_seg_start >= s->end + s->post) {
                        logger->debug(
                            "Segmenter's data was already fully uploaded, "
                            "finalizing.");
                        s->processed = s->finished = true;
                        __send_timeline_synchronization_status(100, s);
                        // return true means we are updated the segments and
                        // caller should call us again
                        return true;
                    }
                }
            }
        }
        return false;
    }

public:
    typedef std::shared_ptr<segmented_uploader> ptr;

    static ptr create(const proto::access_token& access_token,
                      transport::libwebsockets::http::ptr http = nullptr,
                      direct_upload_cb direct_upload_cb = nullptr,
                      send_command_cb send_command_cb = nullptr) {
        struct make_shared_enabler : public segmented_uploader {
            make_shared_enabler(
                const proto::access_token& access_token,
                transport::libwebsockets::http::ptr http = nullptr,
                direct_upload_cb _direct_upload_cb = nullptr,
                send_command_cb _send_command_cb = nullptr)
                : segmented_uploader(access_token,
                                     std::move(http),
                                     std::move(_direct_upload_cb),
                                     std::move(_send_command_cb)) {}
        };
        return std::make_shared<make_shared_enabler>(
            access_token, std::move(http), std::move(direct_upload_cb),
            std::move(send_command_cb));
    }

    bool start() {
        // Does nothing if it was already started outside, returns false,
        // otherwise we need to stop it by ourselves
        need_stop_http_ = http_->start();

        loop_ = http_->schedule_timed_cb(
            std::bind(&segmented_uploader::__async_loop, this), 1);
        return !!loop_;
    }

    void stop() {
        for (auto& t : scheduled_sync_starts_)
            http_->cancel_timed_cb(t.second);
        scheduled_sync_starts_.clear();

        if (loop_) {
            http_->cancel_timed_cb(loop_);
            loop_ = nullptr;
        }

        if (need_stop_http_) {
            http_->stop();
            need_stop_http_ = false;
        }

        // Cleanup all segmenters
        __purge_segmenters(false, false, segmenters_.size());
    }

    bool schedule_upload(segmenter_ptr seg) {
        using namespace std::chrono;

        logger->debug(
            "Schedule processing of the segmenter for {} - {}; pre: {}, post: "
            "{}",
            utils::time::to_iso(seg->begin), utils::time::to_iso(seg->end),
            duration_cast<milliseconds>(seg->pre).count(),
            duration_cast<milliseconds>(seg->post).count());

        scheduled_sync_starts_[seg] = http_->schedule_timed_cb(
            std::bind(&segmented_uploader::__segmenter_enqueue, this, seg),
            std::chrono::duration_cast<std::chrono::milliseconds>(seg->delay)
                .count());

        return true;
    }

    void finalize_upload(segmenter_ptr seg, cloud::time end) {
        logger->debug("Finalize segmenter which starts at {} with end {}",
                      utils::time::to_iso(seg->begin),
                      utils::time::to_iso(end));

        http_->schedule_timed_cb(
            std::bind(&segmented_uploader::__segmenter_finalize, this, seg,
                      end),
            0);
    }

    void cancel_upload(const std::string& ticket) {
        logger->debug("Canceling all segmenters with ticket '{}'", ticket);

        http_->schedule_timed_cb(
            std::bind(&segmented_uploader::__segmenter_cancel_by_ticket, this,
                      ticket),
            0);
    }
};
}  // namespace agent
}  // namespace cloud
}  // namespace vxg