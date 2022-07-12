//  Copyright © 2022 VXG Inc. All rights reserved.
//  Contact: https://www.videoexpertsgroup.com/contact-vxg/
//  This file is part of the demonstration of the VXG Cloud Platform.
//
//  Commercial License Usage
//  Licensees holding valid commercial VXG licenses may use this file in
//  accordance with the commercial license agreement provided with the
//  Software or, alternatively, in accordance with the terms contained in
//  a written agreement between you and VXG Inc. For further information
//  use the contact form at https://www.videoexpertsgroup.com/contact-vxg/

#pragma once

extern "C" {
#define __STDC_CONSTANT_MACROS
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>
}

#include <mutex>

#include <streamer/base_streamer.h>
#include <utils/logging.h>
#include <utils/utils.h>

//! @file ffmpeg_common.h FFmpeg logging, headers and common stuff

namespace vxg {
namespace media {
namespace ffmpeg {

#define LOG_BUF_PREFIX_SIZE 512
#define LOG_BUF_SIZE 1024

//! @brief common ffmpeg class
class common {
    static std::once_flag ff_static_init_flag_;
    vxg::logger::logger_ptr logger;

protected:
    static void ff_log_callback(void* ptr,
                                int level,
                                const char* fmt,
                                va_list vargs) {
        char ffmpeg_log_buf_prefix[LOG_BUF_PREFIX_SIZE];
        char ffmpeg_log_buf_[LOG_BUF_SIZE];
        AVClass* avc = ptr ? *(AVClass**)ptr : NULL;

        // Drop all noisy dhav demuxer's messages.
        // !Yes it uses wrong AV_CLASS_CATEGORY_MUXER category!
        if (avc && avc->category == AV_CLASS_CATEGORY_MUXER &&
            !std::strcmp(avc->class_name, "AVFormatContext") &&
            ((AVFormatContext*)ptr)->iformat &&
            !std::strcmp(((AVFormatContext*)ptr)->iformat->name, "dhav"))
            return;

        if (level > AV_LOG_INFO)
            return;

        std::memset(ffmpeg_log_buf_prefix, 0, LOG_BUF_PREFIX_SIZE);
        std::memset(ffmpeg_log_buf_, 0, LOG_BUF_SIZE);

        std::snprintf(ffmpeg_log_buf_prefix, LOG_BUF_PREFIX_SIZE, "%s", fmt);
        std::vsnprintf(ffmpeg_log_buf_, LOG_BUF_SIZE, ffmpeg_log_buf_prefix,
                       vargs);

        if (level <= AV_LOG_ERROR)
            vxg::logger::instance("ffmpeg")->error(ffmpeg_log_buf_);
        else if (level <= AV_LOG_WARNING)
            vxg::logger::instance("ffmpeg")->warn(ffmpeg_log_buf_);
        else if (level <= AV_LOG_INFO)
            vxg::logger::instance("ffmpeg")->info(ffmpeg_log_buf_);
        else if (level <= AV_LOG_DEBUG)
            vxg::logger::instance("ffmpeg")->debug(ffmpeg_log_buf_);
        else if (level <= AV_LOG_TRACE)
            vxg::logger::instance("ffmpeg")->trace(ffmpeg_log_buf_);
    }

    static void init() {
        av_log_set_flags(AV_LOG_SKIP_REPEATED);
        av_log_set_callback(ff_log_callback);
    }

    static void log_packet(std::string log_ctx,
                           const AVFormatContext* fmt_ctx,
                           const AVPacket* pkt) {
        AVRational* time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
        char pts[AV_TS_MAX_STRING_SIZE], pts_time[AV_TS_MAX_STRING_SIZE];
        char dts[AV_TS_MAX_STRING_SIZE], dts_time[AV_TS_MAX_STRING_SIZE];
        char dur[AV_TS_MAX_STRING_SIZE], dur_time[AV_TS_MAX_STRING_SIZE];
        char clock[AV_TS_MAX_STRING_SIZE];
        AVRational clock_microsec = {1, 1000000};

        av_ts_make_string(pts, pkt->pts);
        av_ts_make_time_string(pts_time, pkt->pts, time_base);
        av_ts_make_string(dts, pkt->dts);
        av_ts_make_time_string(dts_time, pkt->dts, time_base);
        av_ts_make_string(dur, pkt->duration);
        av_ts_make_time_string(dur_time, pkt->duration, time_base);
        av_ts_make_time_string(clock, fmt_ctx->last_absolute_timestamp,
                               &clock_microsec);

        vxg::logger::instance(log_ctx)->debug(
            "pts:{} pts_time:{} dts:{} dts_time:{} duration:{} "
            "duration_time:{} clock:{} "
            "stream_index:{} data: {:np}",
            pts, pts_time, dts, dts_time, dur, dur_time,
            vxg::cloud::utils::time::to_iso(
                std::chrono::time_point<std::chrono::system_clock,
                                        cloud::duration>(
                    std::chrono::microseconds(
                        fmt_ctx->last_absolute_timestamp))),
            pkt->stream_index, spdlog::to_hex(pkt->data, pkt->data + 10));
    }

private:
    static int _ff_interrupt_cb(void* ctx) {
        ffmpeg::common* common = reinterpret_cast<ffmpeg::common*>(ctx);
        return common->get_ff_interrupted();
    }

    struct buffer_data {
        uint8_t* ptr {nullptr};
        ptrdiff_t pos {0};
        ptrdiff_t max_write_pos {0};
        size_t size {0};
        std::shared_ptr<std::vector<uint8_t>> user_buffer {nullptr};
    };

    static int64_t __ff_seek(void* opaque, int64_t offset, int whence) {
        struct buffer_data* bd = (struct buffer_data*)opaque;
        switch (whence) {
            case SEEK_SET:
                return (bd->pos = offset);
                break;
            case SEEK_CUR:
                return (bd->pos += offset);
                break;
            case SEEK_END:
                return (bd->pos = (bd->user_buffer->size() + offset));
                break;
            case AVSEEK_SIZE:
                return bd->user_buffer->size();
                break;
            default:
                break;
        }
        return -1;
    }

    static int __ff_write_packet(void* opaque, uint8_t* buf, int buf_size) {
        struct buffer_data* bd = (struct buffer_data*)opaque;
        vxg::logger::instance("avio-mem-ctx")
            ->trace("write packet pkt_size:{} cur_pos:{} buf_size:{}", buf_size,
                    bd->pos, bd->user_buffer->size());
        if (buf_size < 0)
            return buf_size;

        if (bd->pos + (size_t)buf_size > bd->user_buffer->size()) {
            try {
                size_t new_size =
                    std::max(2 * bd->user_buffer->size(), (size_t)4096);
                bd->user_buffer->resize(new_size);
            } catch (...) {
                return AVERROR(ENOMEM);
            }
        }

        std::memcpy(&bd->user_buffer->data()[bd->pos], buf, buf_size);
        bd->pos += buf_size;
        // Save max write pos and resize user_buffer on close
        bd->max_write_pos = std::max(bd->max_write_pos, bd->pos);

        return buf_size;
    }

    static int __ff_read_packet(void* opaque, uint8_t* buf, int buf_size) {
        struct buffer_data* bd = (struct buffer_data*)opaque;
        buf_size =
            std::min((size_t)buf_size, (bd->user_buffer->size() - bd->pos));

        if (!buf_size)
            return AVERROR_EOF;

        vxg::logger::instance("avio-mem-ctx")
            ->trace("Read {} bytes packets from pos:{} size:{}", buf_size,
                    bd->pos, bd->user_buffer->size());

        /* copy internal buffer data to buf */
        std::memcpy(buf, &bd->user_buffer->data()[bd->pos], buf_size);
        bd->pos += buf_size;

        return buf_size;
    }

    bool _ff_open_output() {
        if (ff_format_ctx_->nb_streams == 0) {
            logger->error("No compatible streams for output format {}",
                          ff_fmt_);
            return false;
        }

        last_out_dts = (int64_t*)av_mallocz_array(ff_format_ctx_->nb_streams,
                                                  sizeof(int64_t));
        last_out_pts = (int64_t*)av_mallocz_array(ff_format_ctx_->nb_streams,
                                                  sizeof(int64_t));
        first_out_pts = (int64_t*)av_mallocz_array(ff_format_ctx_->nb_streams,
                                                   sizeof(int64_t));
        first_out_dts = (int64_t*)av_mallocz_array(ff_format_ctx_->nb_streams,
                                                   sizeof(int64_t));

        for (size_t i = 0; i < ff_format_ctx_->nb_streams; i++) {
            last_out_dts[i] = AV_NOPTS_VALUE;
            last_out_pts[i] = AV_NOPTS_VALUE;
            first_out_dts[i] = AV_NOPTS_VALUE;
            first_out_pts[i] = AV_NOPTS_VALUE;
        }

        av_dump_format(ff_format_ctx_, 0, ff_url_.c_str(), 1);

        if (user_buffer_ == nullptr) {
            if (!(ff_output_format_ctx_->flags & AVFMT_NOFILE)) {
                if (avio_open2(
                        &ff_format_ctx_->pb, ff_url_.c_str(), AVIO_FLAG_WRITE,
                        &ff_format_ctx_->interrupt_callback, &ff_options_)) {
                    logger->error("Could not open output file '{}'",
                                  ff_url_.c_str());
                    return false;
                }
            }
        } else {
            // Memory buffer output

            // Alloc internal avio buffer, ownership transferred to avio_ctx.
            // We should av_freep(&avio_ctx_->buffer) on close.
            avio_ctx_buffer_ =
                static_cast<uint8_t*>(av_malloc(avio_ctx_buffer_size_));
            if (!avio_ctx_buffer_) {
                logger->error("Unable to allocate avio input buffer");
                return false;
            }

            buffer_data_.ptr = user_buffer_->data();
            buffer_data_.pos = 0;
            buffer_data_.size = user_buffer_->size();
            buffer_data_.user_buffer = user_buffer_;

            avio_ctx_ = avio_alloc_context(
                avio_ctx_buffer_, avio_ctx_buffer_size_, 1, &buffer_data_, NULL,
                __ff_write_packet, __ff_seek);
            if (!avio_ctx_) {
                logger->error("Unable to allocate avio input context");
                return false;
            }

            ff_format_ctx_->pb = avio_ctx_;
            ff_format_ctx_->flags |= AVFMT_FLAG_CUSTOM_IO;
        }

        int error;
        if ((error = avformat_write_header(ff_format_ctx_, &ff_options_)) < 0) {
            logger->error("Unable to write output stream header");
            return false;
        }
        // write_header was successfull, trailer can be written
        ff_need_write_trailer_ = true;

        return true;
    }

    void _ff_close_output() {
        if (ff_format_ctx_ && ff_need_write_trailer_ &&
            !(ff_output_format_ctx_->flags & AVFMT_NOFILE)) {
            ff_need_write_trailer_ = false;
            av_write_trailer(ff_format_ctx_);
        }

        if (ff_format_ctx_ && !(ff_output_format_ctx_->flags & AVFMT_NOFILE) &&
            ff_format_ctx_->pb && !user_buffer_) {
            avio_closep(&ff_format_ctx_->pb);
        }

        if (avio_ctx_) {
            if (user_buffer_)
                av_freep(&avio_ctx_->buffer);
            avio_context_free(&avio_ctx_);
        }

        // Resize buffer to the max write pos
        if (user_buffer_)
            user_buffer_->resize(buffer_data_.max_write_pos);
        user_buffer_.reset();

        if (last_out_dts) {
            av_freep(&last_out_dts);
            last_out_dts = nullptr;
        }
        if (last_out_pts) {
            av_freep(&last_out_pts);
            last_out_pts = nullptr;
        }
        if (first_out_pts) {
            av_freep(&first_out_pts);
            first_out_pts = nullptr;
        }
        if (first_out_dts) {
            av_freep(&first_out_dts);
            first_out_dts = nullptr;
        }
    }

    bool _ff_alloc_output_context() {
        if (user_buffer_ == nullptr)
            avformat_alloc_output_context2(
                &ff_format_ctx_, NULL, ff_fmt_.empty() ? NULL : ff_fmt_.c_str(),
                ff_url_.c_str());
        else
            avformat_alloc_output_context2(&ff_format_ctx_, NULL,
                                           ff_fmt_.c_str(), NULL);
        if (!ff_format_ctx_) {
            logger->error("Unable to allocate ffmpeg output format");
            return false;
        }

        ffmpeg::common::reset_interrupt_cb();

        ff_output_format_ctx_ = ff_format_ctx_->oformat;
        ff_need_write_trailer_ = false;

        return true;
    }

    void _ff_free_output_context() {
        if (ff_format_ctx_) {
            avformat_free_context(ff_format_ctx_);
            ff_format_ctx_ = nullptr;
        }
    }

    bool _ff_open_input(
        std::string url,
        AVDictionary* opts,
        std::string fmt,
        std::shared_ptr<std::vector<uint8_t>> input_buffer = nullptr) {
        int i;
        AVDictionary* _opts = nullptr;

        ff_format_ctx_ = avformat_alloc_context();
        if (!ff_format_ctx_) {
            logger->error("Unable to allocate ffmpeg format context");
            return false;
        }

        ffmpeg::common::reset_interrupt_cb();

        if (opts)
            av_dict_copy(&_opts, opts, 0);

        // Regular input, url or file
        if (input_buffer == nullptr) {
            if ((i = avformat_open_input(
                     &ff_format_ctx_, url.c_str(),
                     fmt.empty() ? 0 : av_find_input_format(fmt.c_str()),
                     &_opts)) != 0) {
                char error[128];
                av_strerror(i, error, sizeof(error));
                logger->error("Unable to open {} : {}", url.c_str(), error);
                return false;
            }
        } else {
            // Memory buffer input
            // Ref input shared_ptr buffer here, will be unrefed in
            // _ff_close_input()
            user_buffer_ = input_buffer;

            avio_ctx_buffer_ =
                static_cast<uint8_t*>(av_malloc(avio_ctx_buffer_size_));
            if (!avio_ctx_buffer_) {
                logger->error("Unable to allocate avio input buffer");
                return false;
            }

            buffer_data_.ptr = user_buffer_->data();
            buffer_data_.pos = 0;
            buffer_data_.max_write_pos = 0;
            buffer_data_.size = user_buffer_->size();
            buffer_data_.user_buffer = input_buffer;

            avio_ctx_ =
                avio_alloc_context(avio_ctx_buffer_, avio_ctx_buffer_size_, 0,
                                   &buffer_data_, __ff_read_packet, NULL, NULL);
            if (!avio_ctx_) {
                logger->error("Unable to allocate avio input context");
                return false;
            }
            ff_format_ctx_->pb = avio_ctx_;

            if (avformat_open_input(&ff_format_ctx_, NULL, NULL, NULL) < 0) {
                logger->error("Could not open input");
                return false;
            }
        }

        if (suggested_input_streams_.empty()) {
            if (avformat_find_stream_info(ff_format_ctx_, 0) < 0) {
                logger->error("No streams found");
                return false;
            }
        } else if (!ff_format_ctx_->nb_streams &&
                   suggested_input_streams_.size()) {
            // Some formats contain partial or full tracks info we obtain during
            // the avformat_open_input(), some don't have such info and
            // we add streams by hands according to suggested by user formats
            // if user provided info
            logger->warn(
                "No streams were detected during early handshake, adding "
                "streams provided by user as suggestion");

            for (auto stream_info : suggested_input_streams_) {
                if (!ff_add_stream(stream_info, false)) {
                    logger->warn("Unable to add stream");
                }
            }
        }
        av_dict_free(&_opts);

        for (i = 0; i < static_cast<int>(ff_format_ctx_->nb_streams); i++) {
            AVCodecParameters* codecpar = ff_format_ctx_->streams[i]->codecpar;

            if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
                _AVCodecID_to_VideoCodec(codecpar->codec_id) !=
                    Streamer::StreamInfo::VideoCodec::VC_UNKNOWN) {
                ff_video_stream_index_ = i;

                auto suggested_info = std::find_if(
                    suggested_input_streams_.begin(),
                    suggested_input_streams_.end(),
                    [&](const Streamer::StreamInfo& info) {
                        return info.type ==
                               Streamer::StreamInfo::StreamType::ST_VIDEO;
                    });

                if (suggested_info != suggested_input_streams_.end()) {
                    if (codecpar->width <= 0 || codecpar->height <= 0) {
                        codecpar->width = suggested_info->video.width;
                        codecpar->height = suggested_info->video.height;

                        logger->warn(
                            "ffmpeg was not able to detect stream resolution, "
                            "using suggested by user {}x{}",
                            codecpar->width, codecpar->height);
                    }
                }
            }

            if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO &&
                _AVCodecID_to_AudioCodec(codecpar->codec_id) !=
                    Streamer::StreamInfo::AudioCodec::AC_UNKNOWN)
                ff_audio_stream_index_ = i;

            if (codecpar->codec_type == AVMEDIA_TYPE_DATA)
                ff_data_stream_index_ = i;

            if (codecpar->extradata_size > 0) {
                extradata_[i].assign(
                    codecpar->extradata,
                    codecpar->extradata + codecpar->extradata_size);
            }
        }

        if (ff_video_stream_index_ == -1) {
            logger->warn("video stream not found");
        }

        if (ff_audio_stream_index_ == -1) {
            logger->warn("audio stream not found");
        }

        av_dump_format(ff_format_ctx_, 0, url.c_str(), 0);

        _ff_open_parsers();

        return true;
    }

    void _ff_close_input() {
        _ff_close_parsers();

        if (ff_format_ctx_) {
            avformat_close_input(&ff_format_ctx_);
        }

        if (avio_ctx_) {
            if (user_buffer_)
                av_freep(&avio_ctx_->buffer);
            avio_context_free(&avio_ctx_);
        }

        user_buffer_.reset();
    }

    void _ff_open_parsers() {
        if (ff_video_stream_index_ != -1) {
            // ff_dumpextra_bsf_ = av_bitstream_filter_init("dump_extra");
            if (ff_dumpextra_bsf_) {
                av_opt_set(ff_dumpextra_bsf_->priv_data, "freq", "keyframe", 0);
            }
        }
    }

    void _ff_close_parsers() {
        if (ff_dumpextra_bsf_) {
            // av_bitstream_filter_close(ff_dumpextra_bsf_);
            ff_dumpextra_bsf_ = nullptr;
        }
    }

    void _check_fix_ts(AVPacket* pkt) {
        auto st = ff_format_ctx_->streams[pkt->stream_index];

        if (pkt->dts != AV_NOPTS_VALUE && pkt->pts != AV_NOPTS_VALUE &&
            pkt->dts > pkt->pts) {
            logger->warn(
                "Invalid DTS: {} PTS: {} in output stream %d:%d, replacing by "
                "guess\n",
                pkt->dts, pkt->pts, pkt->stream_index);
            pkt->pts = pkt->dts =
                pkt->pts + pkt->dts + last_out_dts[pkt->stream_index] + 1 -
                FFMIN3(pkt->pts, pkt->dts,
                       last_out_dts[pkt->stream_index] + 1) -
                FFMAX3(pkt->pts, pkt->dts, last_out_dts[pkt->stream_index] + 1);
        }

        if ((st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO ||
             st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO ||
             st->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE) &&
            pkt->dts != AV_NOPTS_VALUE &&
            last_out_dts[pkt->stream_index] != AV_NOPTS_VALUE) {
            // Trying to fix dts if necessary
            int64_t max = last_out_dts[pkt->stream_index] +
                          !(ff_output_format_ctx_->flags & AVFMT_TS_NONSTRICT);

            // Trying to fix non-monotonic DTS the way container allows it, some
            // containers require non-decreasing ts and some require strictly
            // increasing ts, flag AVFMT_TS_NONSTRICT indicates it.
            if (pkt->dts != AV_NOPTS_VALUE &&
                last_out_dts[pkt->stream_index] != AV_NOPTS_VALUE &&
                pkt->dts < max) {
                logger->warn(
                    "Non-monotonic DTS for in stream {}; previous: {} current "
                    "{}, changing to {}.",
                    pkt->stream_index, last_out_dts[pkt->stream_index],
                    pkt->dts, max);
                if (pkt->pts >= pkt->dts)
                    pkt->pts = FFMAX(pkt->pts, max);
                pkt->dts = max;
            }
        }

        if (pkt->pts != AV_NOPTS_VALUE &&
            first_out_pts[pkt->stream_index] == AV_NOPTS_VALUE) {
            first_out_pts[pkt->stream_index] = pkt->pts;
        }
        if (pkt->dts != AV_NOPTS_VALUE &&
            first_out_dts[pkt->stream_index] == AV_NOPTS_VALUE) {
            first_out_dts[pkt->stream_index] = pkt->dts;
        }

        if (first_out_pts[pkt->stream_index] != AV_NOPTS_VALUE) {
            pkt->pts -= first_out_pts[pkt->stream_index];
        }

        if (first_out_dts[pkt->stream_index] != AV_NOPTS_VALUE) {
            pkt->dts -= first_out_dts[pkt->stream_index];
        }

        last_out_dts[pkt->stream_index] = pkt->dts;
        last_out_pts[pkt->stream_index] = pkt->pts;
    }

    bool _ff_add_stream(Streamer::StreamInfo& info, bool is_output) {
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

        if (is_output && codec_id != AV_CODEC_ID_NONE) {
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
                    stream->codecpar->extradata_size =
                        info.video.extradata.size();
                    stream->codecpar->extradata = static_cast<uint8_t*>(
                        av_malloc(info.video.extradata.size() +
                                  AV_INPUT_BUFFER_PADDING_SIZE));
                    if (!stream->codecpar->extradata) {
                        logger->error(
                            "No memory to allocate {} bytes for video "
                            "extradata",
                            info.video.extradata.size());
                        return false;
                    }

                    std::copy(info.video.extradata.begin(),
                              info.video.extradata.end(),
                              stream->codecpar->extradata);
                }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
                stream->codec->flags2 |= AV_CODEC_FLAG2_LOCAL_HEADER;
#pragma GCC diagnostic pop
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
                    stream->codecpar->extradata_size =
                        info.audio.extradata.size();
                    stream->codecpar->extradata = static_cast<uint8_t*>(
                        av_malloc(info.audio.extradata.size() +
                                  AV_INPUT_BUFFER_PADDING_SIZE));
                    if (!stream->codecpar->extradata) {
                        logger->error(
                            "No memory to allocate %ld bytes of audio "
                            "extradata",
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
                logger->warn("Stream type {} not supported, skipping.",
                             info.type);
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

protected:
    common(vxg::logger::logger_ptr _logger,
           std::vector<Streamer::StreamInfo> suggested_input_streams = {})
        : suggested_input_streams_ {suggested_input_streams} {
        // Init static ffmpeg stuff only once per process lifetime
        try {
            std::call_once(ff_static_init_flag_, common::init);
        } catch (...) {
        }

        logger = _logger;
    }

#ifdef av_err2str
#undef av_err2str
    static char* av_err2str(int errnum) {
        static char str[AV_ERROR_MAX_STRING_SIZE];
        memset(str, 0, sizeof(str));
        return av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, errnum);
    }
#endif

    void reset_interrupt_cb() {
        // Reset interrupt callback
        ff_format_ctx_->interrupt_callback.callback = _ff_interrupt_cb;
        ff_format_ctx_->interrupt_callback.opaque = this;
        ff_interrupted_ = false;
    }

    void set_ff_interrupted(bool b) { ff_interrupted_ = b; }
    bool get_ff_interrupted() { return ff_interrupted_; }
    void set_ff_opts(AVDictionary* opts) { ff_options_ = opts; }
    AVDictionary* get_ff_opts() { return ff_options_; }
    bool ff_open_input(
        const char* url,
        AVDictionary* opts,
        std::string fmt,
        std::shared_ptr<std::vector<uint8_t>> input_buffer = nullptr) {
        return _ff_open_input(url, opts, fmt, input_buffer);
    }
    void ff_close_input() { _ff_close_input(); }

    bool ff_alloc_output_context() { return _ff_alloc_output_context(); }
    void ff_free_output_context() { _ff_free_output_context(); }
    bool ff_open_output() { return _ff_open_output(); }
    void ff_close_output() { _ff_close_output(); }
    void ff_check_fix_ts(AVPacket* pkt) { _check_fix_ts(pkt); }

    Streamer::StreamInfo::VideoCodec _AVCodecID_to_VideoCodec(AVCodecID& c) {
        switch (c) {
            case AV_CODEC_ID_H264:
                return Streamer::StreamInfo::VC_H264;
            default:
                return Streamer::StreamInfo::VideoCodec::VC_UNKNOWN;
        }
        return Streamer::StreamInfo::VideoCodec::VC_UNKNOWN;
    }

    Streamer::StreamInfo::AudioCodec _AVCodecID_to_AudioCodec(AVCodecID& c) {
        switch (c) {
            case AV_CODEC_ID_AAC:
                return Streamer::StreamInfo::AC_AAC;
            case AV_CODEC_ID_PCM_ALAW:
                return Streamer::StreamInfo::AC_G711_A;
            case AV_CODEC_ID_PCM_MULAW:
                return Streamer::StreamInfo::AC_G711_U;
            default:
                return Streamer::StreamInfo::AudioCodec::AC_UNKNOWN;
        }
        return Streamer::StreamInfo::AudioCodec::AC_UNKNOWN;
    }

    AVCodecID _VideoCodec_to_AVCodecID(Streamer::StreamInfo::VideoCodec& c) {
        switch (c) {
            case Streamer::StreamInfo::VC_H264:
                return AV_CODEC_ID_H264;
            default:
                return AV_CODEC_ID_NONE;
        }
        return AV_CODEC_ID_NONE;
    }

    AVCodecID _AudioCodec_to_AVCodecID(Streamer::StreamInfo::AudioCodec& c) {
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

    cloud::duration duration() {
        double max_time = 0;
        for (size_t i = 0; i < ff_format_ctx_->nb_streams; i++) {
            AVRational* tb = &ff_format_ctx_->streams[i]->time_base;
            max_time = std::max(
                (last_out_pts[i] - first_out_pts[i]) * av_q2d(*tb), max_time);
        }
        return std::chrono::milliseconds((int)(max_time * 1000));
    }

    bool ff_add_stream(Streamer::StreamInfo& info, bool is_output) {
        return _ff_add_stream(info, is_output);
    }

protected:
    const AVOutputFormat* ff_output_format_ctx_ {nullptr};
    AVDictionary* ff_options_ {nullptr};
    bool ff_interrupted_ {false};
    AVFormatContext* ff_format_ctx_ {nullptr};
    int ff_video_stream_index_ {-1};
    int ff_audio_stream_index_ {-1};
    int ff_data_stream_index_ {-1};
    std::string ff_url_;
    std::string ff_fmt_;
    AVBitStreamFilterContext* ff_dumpextra_bsf_ {nullptr};
    bool ff_need_write_trailer_ {false};

    std::map<int, std::vector<uint8_t>> extradata_;

    // Buffer input
    std::shared_ptr<std::vector<uint8_t>> user_buffer_ {nullptr};
    uint8_t* avio_ctx_buffer_ {nullptr};
    size_t avio_ctx_buffer_size_ {4096};
    AVIOContext* avio_ctx_ {nullptr};
    struct buffer_data buffer_data_;
    int64_t* last_out_dts {nullptr};
    int64_t* last_out_pts {nullptr};
    int64_t* first_out_dts {nullptr};
    int64_t* first_out_pts {nullptr};
    std::vector<Streamer::StreamInfo> suggested_input_streams_;
};
}  // namespace ffmpeg
}  // namespace media
}  // namespace vxg
