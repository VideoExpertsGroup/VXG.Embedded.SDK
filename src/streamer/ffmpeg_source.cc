#include <streamer/ffmpeg_sink.h>
#include <streamer/ffmpeg_source.h>
#include <iomanip>
#include <iostream>

namespace vxg {
namespace media {

ffmpeg::Source::Source(
    std::vector<Streamer::StreamInfo> suggested_input_streams)
    : ffmpeg::common(vxg::logger::instance(name()), suggested_input_streams) {
    vxg::logger::set_level(logger, vxg::logger::lvl_warn);
}
ffmpeg::Source::~Source() {}

bool ffmpeg::Source::init(std::string url,
                          AVDictionary* opts,
                          std::string fmt) {
    return ffmpeg::common::ff_open_input(url.c_str(), opts, fmt);
}

bool ffmpeg::Source::init(std::shared_ptr<std::vector<uint8_t>> input_buffer,
                          AVDictionary* opts,
                          std::string fmt) {
    return ffmpeg::common::ff_open_input("", opts, fmt, input_buffer);
}

bool ffmpeg::Source::init(std::string url) {
    return init(url, nullptr);
}

void ffmpeg::Source::finit() {
    ffmpeg::common::ff_close_input();
}

std::vector<Streamer::StreamInfo> ffmpeg::Source::negotiate() {
    std::vector<Streamer::StreamInfo> streams_info;

    for (size_t i = 0; ff_format_ctx_ && i < ff_format_ctx_->nb_streams; i++) {
        Streamer::StreamInfo info;
        AVCodecParameters* codecpar = ff_format_ctx_->streams[i]->codecpar;
        switch (codecpar->codec_type) {
            case AVMEDIA_TYPE_VIDEO:
                if (_AVCodecID_to_VideoCodec(codecpar->codec_id) ==
                    Streamer::StreamInfo::VideoCodec::VC_UNKNOWN)
                    continue;
                /* Video info */
                info.type = Streamer::StreamInfo::ST_VIDEO;
                info.video.codec = _AVCodecID_to_VideoCodec(codecpar->codec_id);
                info.video.width = codecpar->width;
                info.video.height = codecpar->height;
                info.video.bitrate = codecpar->bit_rate;
                info.video.timebase.first =
                    ff_format_ctx_->streams[i]->time_base.num;
                info.video.timebase.second =
                    ff_format_ctx_->streams[i]->time_base.den;
                info.video.extradata.assign(
                    codecpar->extradata,
                    codecpar->extradata + codecpar->extradata_size);
                break;
            case AVMEDIA_TYPE_AUDIO:
                if (_AVCodecID_to_AudioCodec(codecpar->codec_id) ==
                    Streamer::StreamInfo::AudioCodec::AC_UNKNOWN)
                    continue;
                /* Audio info */
                info.type = Streamer::StreamInfo::ST_AUDIO;
                info.audio.codec = _AVCodecID_to_AudioCodec(codecpar->codec_id);
                info.audio.channels = codecpar->channels;
                info.audio.bitrate = codecpar->bit_rate;
                info.audio.samplerate = codecpar->sample_rate;
                info.audio.timebase.first =
                    ff_format_ctx_->streams[i]->time_base.num;
                info.audio.timebase.second =
                    ff_format_ctx_->streams[i]->time_base.den;
                if (codecpar->extradata)
                    info.audio.extradata.assign(
                        codecpar->extradata,
                        codecpar->extradata + codecpar->extradata_size);
                break;
            case AVMEDIA_TYPE_DATA:
                info.type = Streamer::StreamInfo::ST_DATA;
                break;
            default:
                logger->warn("Unknown/unsupported stream {}, skipping", i);
                continue;
                break;
        }

        streams_info.push_back(info);
    }

    return streams_info;
}

std::shared_ptr<Streamer::MediaFrame> ffmpeg::Source::pullFrame() {
    int rread = -1;
    AVPacket packet;
    av_init_packet(&packet);

    if (ff_format_ctx_)
        rread = av_read_frame(ff_format_ctx_, &packet);

    if (rread < 0) {
        logger->warn("av_read_fame() return {} < 0, {}", rread,
                     av_err2str(rread));
        // Generate EOS file source and error for live
        if (!(ff_format_ctx_->iformat->flags & AVFMT_NOFILE))
            error(Streamer::StreamError::E_EOS);
        else
            error(Streamer::StreamError::E_FATAL);
        av_packet_unref(&packet);
        return nullptr;
    }

    log_packet(name(), ff_format_ctx_, &packet);

    if ((packet.stream_index == ff_video_stream_index_ ||
         packet.stream_index == ff_audio_stream_index_ ||
         packet.stream_index == ff_data_stream_index_)) {
        std::shared_ptr<Streamer::MediaFrame> frame(new Streamer::MediaFrame);

        if (packet.stream_index == ff_audio_stream_index_) {
            frame->type = Streamer::MediaType::AUDIO;
        } else if (packet.stream_index == ff_video_stream_index_) {
            auto codecpar =
                ff_format_ctx_->streams[packet.stream_index]->codecpar;
            frame->type = Streamer::MediaType::VIDEO;

            // Apply extradata filter, appends sps/pps to keyframe
            if (ff_dumpextra_bsf_ && (packet.flags & AV_PKT_FLAG_KEY)) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
                av_apply_bitstream_filters(
                    ff_format_ctx_->streams[packet.stream_index]->codec,
                    &packet, ff_dumpextra_bsf_);
#pragma GCC diagnostic pop
            } else if ((packet.flags & AV_PKT_FLAG_KEY) && packet.data &&
                       packet.size > 4 && (packet.data[4] & 0x1f) != 0x7 &&
                       codecpar->extradata && codecpar->extradata[0] != 1) {
                frame->data.insert(
                    frame->data.begin(), codecpar->extradata,
                    codecpar->extradata + codecpar->extradata_size);
            }
        } else if (packet.stream_index == ff_data_stream_index_) {
            frame->type = Streamer::MediaType::DATA;
        }

        frame->data.insert(frame->data.end(), packet.data,
                           packet.data + packet.size);
        frame->len = frame->data.size();
#ifdef SHIFT_TS
        if (packet.pts != AV_NOPTS_VALUE) {
            int64_t pts_diff =
                packet.pts -
                ((ff_format_ctx_->streams[packet.stream_index]->start_time ==
                  AV_NOPTS_VALUE)
                     ? 0
                     : ff_format_ctx_->streams[packet.stream_index]
                           ->start_time);
            frame->pts = pts_diff < 0 ? 0 : pts_diff;
        } else
            frame->pts = Streamer::MediaFrame::NO_PTS;

        if (packet.dts != AV_NOPTS_VALUE) {
            int64_t dts_diff =
                packet.dts -
                ff_format_ctx_->streams[packet.stream_index]->first_dts;
            frame->dts = dts_diff < 0 ? 0 : dts_diff;
        } else
            frame->dts = Streamer::MediaFrame::NO_PTS;
#else
        frame->pts = packet.pts;
        frame->dts = packet.dts;
#endif
        frame->duration = packet.duration;
        frame->is_key = packet.flags & AV_PKT_FLAG_KEY;
        frame->timescale.first =
            ff_format_ctx_->streams[packet.stream_index]->time_base.num;
        frame->timescale.second =
            ff_format_ctx_->streams[packet.stream_index]->time_base.den;
        frame->time_realtime = ff_format_ctx_->last_absolute_timestamp;

        av_packet_unref(&packet);
        return frame;
    }

    av_packet_unref(&packet);
    return nullptr;
}

void ffmpeg::Source::stop() {
    set_ff_interrupted(true);
    logger->debug("Stop {} source", name());
    Streamer::ISource::stop();
}

}  // namespace media
}  // namespace vxg