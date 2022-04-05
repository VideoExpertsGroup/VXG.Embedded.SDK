#include "multifrag_sink.h"
#include <utils/utils.h>
#include <fstream>

namespace vxg {
namespace media {

namespace MultiFrag {

bool Sink::init(std::string url) {
    frag_rotate_stats();
    fragment_start_ = std::chrono::system_clock::from_time_t(0);
    fragment_first_video_pts_ = 0;
    fragment_first_video_dts_ = 0;
    fragment_last_video_pts_ = 0;
    fragment_last_video_dts_ = 0;
    fragment_duration_ = std::chrono::milliseconds(0);
    video_key_frame_received_ = false;

    // TODO: implement queued video/audio pts-sorted pushing to internal sink
    disable_audio_ = true;

    // Internal sink will be started in negotiate()
    return true;
}

bool Sink::finit() {
    stop_internal_sink();

    if (!frag_uri_.empty() &&
        fragment_duration_ > std::chrono::milliseconds(0)) {
        logger->info("Last frag size {} duration {} finalized", size_,
                     fragment_duration_.count());
        fragment_finalized(frag_uri_, fragment_start_, fragment_duration_);
    }

    fragment_start_ = std::chrono::system_clock::from_time_t(0);
    fragment_first_video_pts_ = 0;
    fragment_first_video_dts_ = 0;
    fragment_last_video_pts_ = 0;
    fragment_last_video_dts_ = 0;
    fragment_duration_ = std::chrono::milliseconds(0);
    video_key_frame_received_ = false;

    return true;
}

bool Sink::negotiate(std::vector<Streamer::StreamInfo> streams_info) {
    // FIXME: add audio support
    if (disable_audio_)
        streams_info.erase(
            std::remove_if(streams_info.begin(), streams_info.end(),
                           [](const Streamer::StreamInfo& info) {
                               return info.type ==
                                      Streamer::StreamInfo::ST_AUDIO;
                           }),
            streams_info.end());

    stream_info_ = streams_info;

    return start_internal_sink();
}

bool Sink::init_internal_sink(size_t counter) {
    if (internal_sink_) {
        frag_uri_ = fragment_generate_uri(counter);
        return internal_sink_->init(frag_uri_);
    } else
        return false;
}

bool Sink::finit_internal_sink(size_t counter) {
    if (internal_sink_)
        return internal_sink_->finit();
    else
        return false;
}

bool Sink::start_internal_sink() {
    if (!init_internal_sink(counter_))
        return false;

    queue_->flush_stop();
    internal_sink_->setQueue(queue_);

    if (!internal_sink_->start())
        return false;

    return internal_sink_->negotiate(stream_info_);
}

bool Sink::stop_internal_sink() {
    internal_sink_->stop();

    return finit_internal_sink(counter_);
}

bool Sink::frag_need_finalize(std::shared_ptr<Streamer::MediaFrame> frame) {
    switch (mode_) {
        case Mode::SIZE:
            if (size_ >= mode_param_)
                return true;
            break;
        case Mode::DURATION:
            if (fragment_duration_ >= std::chrono::seconds(mode_param_))
                return true;
            break;
        default:
            break;
    }

    return false;
}

void Sink::frag_update_stats(std::shared_ptr<Streamer::MediaFrame> frame) {
    size_ += frame->len;

    // Duration is current frame timestamp - frag's first frame timestamp
    // timestamp scaled from original timescale to milliseconds here
    fragment_duration_ = std::chrono::milliseconds(
        frame->timescale.first * 1000 *
        (frame->pts - fragment_first_video_pts_) / frame->timescale.second);
}

void Sink::frag_rotate_stats() {
    // Start of the next fragment
    fragment_start_ += fragment_duration_;
    // Reset duration and size of the next fragment
    fragment_duration_ = std::chrono::milliseconds(0);
    size_ = 0;
    // Bump fragments counter
    counter_++;
}

bool Sink::process(std::shared_ptr<Streamer::MediaFrame> frame) {
    using namespace std::chrono;

    // Copy here because modifying of the object will affect other sinks
    auto new_frame = std::make_shared<Streamer::MediaFrame>(*frame);

    if (frame && frame->type == Streamer::VIDEO) {
        // Wait for keyframe on start
        if (!video_key_frame_received_) {
            if (!frame->is_key) {
                logger->warn("Skipping non key video frame on start.");
                return true;
            } else {
                video_key_frame_received_ = true;
            }
        }

        if (frame->pts != Streamer::MediaFrame::NO_PTS &&
            frame->pts < fragment_last_video_pts_) {
            logger->warn("PTS reverse order, dropping frame and wait for key");
            video_key_frame_received_ = false;
            return true;
        }

        if (fragment_start_ == system_clock::from_time_t(0)) {
            auto clock_time = system_clock::now();
            if (frame->time_realtime != 0 &&
                (clock_time - time_point<system_clock>(
                                  microseconds(frame->time_realtime)) <
                 seconds(30))) {
                fragment_start_ = time_point<system_clock>(
                    microseconds(frame->time_realtime));
                logger->info(
                    "Source provided realworld start time {}, using it as "
                    "first fragment start instead of clock time {} difference "
                    "{} ms",
                    vxg::cloud::utils::time::to_iso(fragment_start_),
                    vxg::cloud::utils::time::to_iso(clock_time),
                    duration_cast<milliseconds>(clock_time - fragment_start_)
                        .count());
            } else {
                fragment_start_ = clock_time;
                logger->info(
                    "Using clock time as first fragment start time {}!",
                    vxg::cloud::utils::time::to_iso(fragment_start_));
            }
        }

        // If current frame is key frame and frag duration or size exceeded we
        // finalize current fragment, notify derived class and start new frag.
        if (frame->pts != Streamer::MediaFrame::NO_PTS && frame->is_key &&
            frag_need_finalize(frame)) {
            stop_internal_sink();
            // Current key frame is not included into fragment_duration_ yet
            fragment_finalized(frag_uri_, fragment_start_, fragment_duration_);

            logger->debug("Fragment size {} duration {} finalized", size_,
                          fragment_duration_.count());
            start_internal_sink();

            frag_rotate_stats();

            fragment_first_video_pts_ = fragment_last_video_pts_;
            fragment_first_video_dts_ = fragment_last_video_dts_;
            fragment_first_audio_pts_ = fragment_last_audio_pts_;
            fragment_first_audio_dts_ = fragment_last_audio_dts_;
        }

        if (fragment_first_video_pts_ == 0 ||
            fragment_first_video_pts_ > frame->pts ||
            fragment_first_video_pts_ == Streamer::MediaFrame::NO_PTS ||
            fragment_first_video_dts_ == Streamer::MediaFrame::NO_PTS) {
            fragment_first_video_pts_ = frame->pts;
            fragment_first_video_dts_ = frame->dts;
        }

        if (fragment_first_video_pts_ == Streamer::MediaFrame::NO_PTS ||
            fragment_first_video_dts_ == Streamer::MediaFrame::NO_PTS) {
            logger->warn("No valid PTS yet, dropping frame");
            return true;
        }

        // Update fragment duration and size stats, only video used in duration
        // calculation
        frag_update_stats(frame);

        if (new_frame->pts != Streamer::MediaFrame::NO_PTS) {
            fragment_last_video_pts_ = new_frame->pts;
            new_frame->pts -= fragment_first_video_pts_;
        }

        if (new_frame->dts != Streamer::MediaFrame::NO_PTS) {
            fragment_last_video_dts_ = new_frame->dts;
            new_frame->dts -= fragment_first_video_dts_;
        }

        // Push video frame to internal sink queue
        queue_->push(new_frame);

        return true;
    } else if (frame && frame->type == Streamer::AUDIO) {
        if (!disable_audio_) {
            if (fragment_first_audio_pts_ == 0) {
                fragment_first_audio_pts_ = frame->pts;
                fragment_first_audio_dts_ = frame->dts;
            }

            fragment_last_audio_pts_ = new_frame->pts;
            fragment_last_audio_dts_ = new_frame->dts;
            new_frame->pts -= fragment_first_audio_pts_;
            new_frame->dts -= fragment_first_audio_dts_;

            // Push audio frame to internal sink queue
            queue_->push(new_frame);
        }

        return true;
    }

    return false;
}

}  // namespace MultiFrag
}  // namespace media
}  // namespace vxg