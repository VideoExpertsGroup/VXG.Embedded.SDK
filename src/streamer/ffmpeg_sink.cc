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
        if (!add_stream(stream_info)) {
            logger->warn("Unable to add stream");
        }
    }

    video_key_frame_received_ = false;

    return ff_open_output();
}

static AVCodecID _VideoCodec_to_AVCodecID(Streamer::StreamInfo::VideoCodec& c) {
    switch (c) {
        case Streamer::StreamInfo::VC_H264:
            return AV_CODEC_ID_H264;
        default:
            return AV_CODEC_ID_NONE;
    }
    return AV_CODEC_ID_NONE;
}

static AVCodecID _AudioCodec_to_AVCodecID(Streamer::StreamInfo::AudioCodec& c) {
    switch (c) {
        case Streamer::StreamInfo::AC_AAC:
            return AV_CODEC_ID_AAC;
        case Streamer::StreamInfo::AC_G711_A:
            return AV_CODEC_ID_PCM_ALAW;
        case Streamer::StreamInfo::AC_G711_U:
            return AV_CODEC_ID_PCM_MULAW;
        default:
            return AV_CODEC_ID_NONE;
    }
    return AV_CODEC_ID_NONE;
}

/* Add an output stream. */
bool ffmpeg::Sink::add_stream(Streamer::StreamInfo& info) {
    AVCodecID codec_id = AV_CODEC_ID_NONE;

    switch (info.type) {
        case Streamer::StreamInfo::ST_VIDEO:
            codec_id = _VideoCodec_to_AVCodecID(info.video.codec);
            break;
        case Streamer::StreamInfo::ST_AUDIO:
            codec_id = _AudioCodec_to_AVCodecID(info.audio.codec);
            break;
        default:
            break;
    }

    if (codec_id != AV_CODEC_ID_NONE) {
        if (avformat_query_codec(ff_format_ctx_->oformat, codec_id,
                                 FF_COMPLIANCE_STRICT) != 1) {
            logger->warn(
                "Stream {} is not compatible with output container {} "
                "format",
                codec_id, ff_fmt_);
            return false;
        }
    }

    /* Will be deleted by avformat_free_context */
    AVStream* stream = avformat_new_stream(ff_format_ctx_, NULL);

    if (!stream) {
        logger->error("Could not allocate stream");
        return false;
    }

    stream->id = ff_format_ctx_->nb_streams - 1;
    switch (info.type) {
        case Streamer::StreamInfo::ST_VIDEO: {
            stream->codecpar->codec_id = codec_id;
            stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
            stream->codecpar->codec_tag = 0;
            stream->codecpar->format = AV_PIX_FMT_YUV420P;
            stream->codecpar->width = info.video.width;
            stream->codecpar->height = info.video.height;
            stream->codecpar->bit_rate = info.video.bitrate;
            stream->time_base = (AVRational) {info.video.timebase.first,
                                              info.video.timebase.second};
            if (!info.video.extradata.empty()) {
                stream->codecpar->extradata_size = info.video.extradata.size();
                stream->codecpar->extradata = static_cast<uint8_t*>(
                    av_malloc(info.video.extradata.size() +
                              AV_INPUT_BUFFER_PADDING_SIZE));
                if (!stream->codecpar->extradata) {
                    logger->error(
                        "No memory to allocate {} bytes for video extradata",
                        info.video.extradata.size());
                    return false;
                }

                std::copy(info.video.extradata.begin(),
                          info.video.extradata.end(),
                          stream->codecpar->extradata);
            }

            stream->codec->flags2 |= AV_CODEC_FLAG2_LOCAL_HEADER;
        } break;
        case Streamer::StreamInfo::ST_AUDIO: {
            stream->codecpar->codec_id = codec_id;
            stream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
            stream->codecpar->codec_tag = 0;
            stream->codecpar->sample_rate = info.audio.samplerate;
            stream->codecpar->channels = info.audio.channels;
            stream->codecpar->bit_rate = info.audio.bitrate;
            stream->time_base = (AVRational) {info.audio.timebase.first,
                                              info.audio.timebase.second};
            if (!info.audio.extradata.empty()) {
                stream->codecpar->extradata_size = info.audio.extradata.size();
                stream->codecpar->extradata = static_cast<uint8_t*>(
                    av_malloc(info.audio.extradata.size() +
                              AV_INPUT_BUFFER_PADDING_SIZE));
                if (!stream->codecpar->extradata) {
                    logger->error(
                        "No memory to allocate %ld bytes of audio extradata",
                        info.video.extradata.size());
                    return false;
                }

                std::copy(info.audio.extradata.begin(),
                          info.audio.extradata.end(),
                          stream->codecpar->extradata);
            }
        } break;
        case Streamer::StreamInfo::ST_DATA: {
            stream->codecpar->codec_type = AVMEDIA_TYPE_DATA;
        } break;
        default:
            logger->warn("Stream type {} not supported, skipping.", info.type);
            return true;
    }

    switch (stream->codecpar->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            logger->info("Audio stream allocated");
            ff_audio_stream_index_ = stream->id;
            break;
        case AVMEDIA_TYPE_VIDEO:
            logger->info("Video stream allocated");
            ff_video_stream_index_ = stream->id;
            break;
        case AVMEDIA_TYPE_DATA:
            logger->info("Data stream allocated");
            ff_data_stream_index_ = stream->id;
            break;
        default:
            break;
    }

    return true;
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
                        logger->warn(
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