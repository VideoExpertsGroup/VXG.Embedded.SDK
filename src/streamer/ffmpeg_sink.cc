#include "ffmpeg_sink.h"

namespace vxg {
namespace media {

ffmpeg::Sink::Sink() : common(vxg::logger::instance(name())) {}
ffmpeg::Sink::~Sink() {}

bool ffmpeg::Sink::finit() {
    ff_close_output();
    ff_free_output_context();
    return true;
}

bool ffmpeg::Sink::negotiate(std::vector<Streamer::StreamInfo> streams_info) {
    if (streams_info.empty()) {
        logger->error("No input streams info");
        return false;
    }

    for (auto stream_info : streams_info) {
        if (!ff_add_stream(stream_info, true)) {
            logger->warn("Unable to add stream");
        }
    }

    video_key_frame_received_ = false;

    return ff_open_output();
}

bool ffmpeg::Sink::init(std::string url,
                        std::string fmt,
                        std::shared_ptr<std::vector<uint8_t>> data_buffer) {
    ff_url_ = url;
    ff_fmt_ = fmt;
    user_buffer_ = data_buffer;

    return ff_alloc_output_context();
}

bool ffmpeg::Sink::init(std::string url) {
    ff_url_ = url;

    return ff_alloc_output_context();
}

bool ffmpeg::Sink::process(std::shared_ptr<Streamer::MediaFrame> frame) {
    int ret;
    AVPacket pkt = {0};
    int stream_index = -1;

    if (frame) {
        switch (frame->type) {
            case Streamer::VIDEO:
            case Streamer::VIDEO_AVC_SPS:
            case Streamer::VIDEO_AVC_PPS:
            case Streamer::VIDEO_SEQ_HDR: {
                stream_index = ff_video_stream_index_;

                // Wait for keyframe on start
                if (!video_key_frame_received_) {
                    if (!frame->is_key) {
                        logger->debug(
                            "Skipping non key video frame type {} on start.",
                            frame->type);
                        return true;
                    } else {
                        video_key_frame_received_ = true;
                    }
                }

            } break;
            case Streamer::AUDIO:
            case Streamer::AUDIO_SEQ_HDR:
                stream_index = ff_audio_stream_index_;
                break;
            case Streamer::DATA:
                stream_index = ff_data_stream_index_;
                break;
            default:
                logger->warn("Skip frame with type {}", frame->type);
                return true;
        }

        if (stream_index == -1) {
            // Incompatible with container stream, just skipping it
            return true;
        }

        pkt.pts = (frame->pts == Streamer::MediaFrame::NO_PTS) ? AV_NOPTS_VALUE
                                                               : frame->pts;
        pkt.dts = (frame->dts == Streamer::MediaFrame::NO_PTS) ? AV_NOPTS_VALUE
                                                               : frame->dts;
        pkt.duration = frame->duration;
        pkt.data = frame->data.data();
        pkt.size = frame->len;
        pkt.stream_index = ff_format_ctx_->streams[stream_index]->index;
        av_packet_rescale_ts(&pkt,
                             {frame->timescale.first, frame->timescale.second},
                             ff_format_ctx_->streams[stream_index]->time_base);
        pkt.flags |= frame->is_key ? AV_PKT_FLAG_KEY : 0;

        // Fix pts/dts if necessary, mainly prevents non-monotonical timestamps
        ff_check_fix_ts(&pkt);

        log_packet(name(), ff_format_ctx_, &pkt);

        ret = av_interleaved_write_frame(ff_format_ctx_, &pkt);
        if (ret < 0) {
            if (ret != AVERROR(EINVAL)) {
                logger->error("Error while writing output packet: {}\n",
                              av_err2str(ret));
                av_packet_unref(&pkt);
                set_eos(true);
                return false;
            }

            logger->warn(
                "Problem while writing output, skipping packet representing {} "
                "{} : {}\n",
                stream_index == ff_video_stream_index_ ? "video" : "audio",
                frame->is_key ? "key frame" : "non-key frame", av_err2str(ret));
        }
        av_packet_unref(&pkt);

        return true;
    }

    return false;
}

// Stopping ffmpeg data feeding
void ffmpeg::Sink::stop() {
    logger->info("Stop sink {}", name());

    // We may be blocked inside ffmpeg writing loop invoked from the process()
    // so we need to interrupt it using ffmpeg's interrupt callback.
    set_ff_interrupted(true);
    // Stop our data feeding thread, process() wont be called anymore.
    Streamer::ISink::stop();
    // Here we are sure wont get more data since our data feeding thread is
    // stopped, but we need to finalize the container in finit() so we disable
    // ffmpeg interruption.
    set_ff_interrupted(false);
}

}  // namespace media
}  // namespace vxg