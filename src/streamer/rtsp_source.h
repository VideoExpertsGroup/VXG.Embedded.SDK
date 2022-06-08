#ifndef __RTSP_SOURCE_H
#define __RTSP_SOURCE_H

#include "ffmpeg_source.h"
#include "stream.h"

//! @file rtsp_source.h RTSP source

namespace vxg {
namespace media {

//! @brief RTSP source class
class rtsp_source : public ffmpeg::Source {
    vxg::logger::logger_ptr logger {vxg::logger::instance(name())};
    bool rtp_over_tcp_ {true};
    std::string rtp_transport_ {"tcp"};
    std::vector<Streamer::MediaType> media_types_ {0};
    const int DEFAULT_TIMEOUT_SEC = 5;
    std::chrono::seconds timeout_;

protected:
    std::map<std::string, std::string> ffmpeg_opts_;

public:
    //! @brief Construct a new rtsp source object
    //!
    //! @param[in] rtp_over_tcp Flag indicates if user wants RTP over TCP
    //! @param[in] media_types List of media types to ask from RTSP server, can
    //!                        be used to filter out unnecessary tracks.
    //!                        If empty all types will be requested.
    rtsp_source(bool rtp_over_tcp = true,
                std::vector<Streamer::MediaType> media_types =
                    std::vector<Streamer::MediaType>(0))
        : rtp_over_tcp_ {rtp_over_tcp},
          media_types_ {media_types},
          timeout_ {std::chrono::seconds(DEFAULT_TIMEOUT_SEC)} {}

    //! @brief Construct a new rtsp source object
    //!
    //! @param[in] rtp_transport RTP transport passed directly to ffmpeg.
    //! @param[in] media_types List of media types to ask from RTSP server, can
    //!                        be used to filter out unnecessary tracks.
    //!                        If empty all types will be requested.
    //! @param[in] ffmpeg_opts Map of ffmpeg options key values pairs.
    //! @param[in] timeout     RTSP client io timeout. Doesn't mean the
    //!                        connection will be closed after this timeout but
    //!                        specifies the amount of time ffmpeg spends in io
    //!                        loop spinning, infinite timeout causes spining
    //!                        forever if connection wasn't closed but no data
    //!                        was received.
    rtsp_source(std::string rtp_transport = "tcp",
                std::vector<Streamer::MediaType> media_types =
                    std::vector<Streamer::MediaType>(0),
                std::map<std::string, std::string> ffmpeg_opts = {},
                std::chrono::seconds timeout = std::chrono::seconds(0))
        : rtp_transport_ {rtp_transport},
          media_types_ {media_types},
          ffmpeg_opts_ {ffmpeg_opts},
          timeout_ {timeout} {
        if (timeout_ == std::chrono::seconds(0))
            timeout_ = std::chrono::seconds(DEFAULT_TIMEOUT_SEC);
    }

    //! @brief Overloaded init method
    //!
    //! @param[in] url RTSP URL link
    //! @return true
    //! @return false
    virtual bool init(std::string url) {
        AVDictionary* opts = NULL;

        for (auto& o : ffmpeg_opts_) {
            av_dict_set(&opts, o.first.c_str(), o.second.c_str(),
                        AV_OPT_SEARCH_CHILDREN);
        }

        // av_dict_set(&opts, "reorder_queue_size", "5000",
        //             AV_OPT_SEARCH_CHILDREN);
        av_dict_set(&opts, "recv_buffer_size", "4194304",
                    AV_OPT_SEARCH_CHILDREN);
        av_dict_set(&opts, "recv_buffer", "4194304", AV_OPT_SEARCH_CHILDREN);
        av_dict_set(&opts, "buffer_size", "4194304", AV_OPT_SEARCH_CHILDREN);
        // av_dict_set(&opts, "analyzeduration", "2000000",
        //             AV_OPT_SEARCH_CHILDREN);
        // av_dict_set(&opts, "probesize", "1000000", AV_OPT_SEARCH_CHILDREN);
        // av_dict_set(&opts, "stimeout", "10000000", AV_OPT_SEARCH_CHILDREN);
        // Options below used during avformat_find_stream_info() with RTPoverUDP
        av_dict_set(&opts, "max_delay", "150000", AV_OPT_SEARCH_CHILDREN);
        // av_dict_set(&opts, "avioflags", "direct", AV_OPT_SEARCH_CHILDREN);
        // av_dict_set(&opts, "fpsprobesize", "2", AV_OPT_SEARCH_CHILDREN);
        // av_dict_set(&opts, "flush_packets", "1", AV_OPT_SEARCH_CHILDREN);
        // av_dict_set(&opts, "skip_estimate_duration_from_pts", "1",
        //             AV_OPT_SEARCH_CHILDREN);
        av_dict_set(&opts, "fflags", "+discardcorrupt", AV_OPT_SEARCH_CHILDREN);

        if (!rtp_transport_.empty())
            av_dict_set(&opts, "rtsp_transport", rtp_transport_.c_str(),
                        AV_OPT_SEARCH_CHILDREN);
        else if (rtp_over_tcp_)
            av_dict_set(&opts, "rtsp_transport", "async_tcp",
                        AV_OPT_SEARCH_CHILDREN);

        bool sparse_only_stream = false;
        if (!media_types_.empty()) {
            std::string media_types_str;
            for (size_t i = 0; i < media_types_.size(); i++) {
                std::string media_type;

                if (media_types_[i] == Streamer::VIDEO)
                    media_type = "video";
                else if (media_types_[i] == Streamer::AUDIO)
                    media_type = "audio";
                else if (media_types_[i] == Streamer::DATA) {
                    media_type = "data";
                    sparse_only_stream = (media_types_.size() == 1);
                } else
                    logger->warn("Unknown media type {}, skipping",
                                 media_types_[i]);

                if (!media_type.empty()) {
                    media_types_str += media_type;
                    // ffmpeg uses '+' as separator for multiple flags
                    if (i < media_types_.size() - 1)
                        media_types_str += "+";
                }
            }

            if (!media_types_str.empty()) {
                logger->info("Requesting '{}' with allowed_media_types '{}'",
                             url, media_types_str);
                av_dict_set(&opts, "allowed_media_types",
                            media_types_str.c_str(), AV_OPT_SEARCH_CHILDREN);
            }
        }

        // ffmpeg udp and tcp transports use different timeout precisions
        int timeout =
            std::strstr(rtp_transport_.c_str(), "udp")
                ? std::chrono::duration_cast<std::chrono::milliseconds>(
                      timeout_)
                      .count()
                : std::chrono::duration_cast<std::chrono::microseconds>(
                      timeout_)
                      .count();

        // Set higher timeout for sparse-only streams like metadata-only.
        // We can't set 0 as it will cause deadlock sooner or later.
        if (sparse_only_stream)
            timeout *= 10;
#if FF_API_OLD_RTSP_OPTIONS
        av_dict_set_int(&opts, "stimeout", timeout, AV_OPT_SEARCH_CHILDREN);
#else
        av_dict_set_int(&opts, "timeout", timeout, AV_OPT_SEARCH_CHILDREN);
#endif

        if (!ffmpeg::Source::init(url, opts)) {
            logger->error("Unable to connect to stream");
            return false;
        }

        return true;
    }

    virtual std::string name() override { return "rtsp-source"; }
};

}  // namespace media
}  // namespace vxg

#endif