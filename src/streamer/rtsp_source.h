#ifndef __RTSP_SOURCE_H
#define __RTSP_SOURCE_H

#include "ffmpeg_source.h"
#include "stream.h"

//! @file rtsp_source.h RTSP source

namespace vxg {
namespace media {

//! @brief RTSP source class
class rtsp_source : public ffmpeg::Source {
public:
    enum transport { UDP, TCP, UDP_MULTICAST, HTTP, HTTPS, ASYNC_TCP };

private:
    vxg::logger::logger_ptr logger {vxg::logger::instance(name())};
    bool rtp_over_tcp_ {true};
    transport rtp_transport_ {transport::TCP};
    std::vector<Streamer::MediaType> media_types_ {0};
    const int DEFAULT_TIMEOUT_SEC = 5;
    std::chrono::seconds timeout_;

protected:
    std::map<std::string, std::string> ffmpeg_opts_;

    const char* __transport_to_ff(transport t) {
        const char* result = "tcp";
        switch (t) {
            case UDP:
                result = "udp";
                break;
            case TCP:
                result = "tcp";
                break;
            case UDP_MULTICAST:
                result = "udp_multicast";
                break;
            case HTTP:
                result = "http";
                break;
            case HTTPS:
                result = "https";
                break;
            case ASYNC_TCP:
                result = "async_tcp";
                break;
            default:
                break;
        }
        return result;
    }

public:
    //! @brief Construct a new rtsp source object
    //!
    //! @param[in] rtp_transport RTSP transport.
    //! @param[in] media_types List of media types to ask from RTSP server, can
    //!                        be used to filter out unnecessary tracks.
    //!                        If empty all types will be requested.
    //! @param[in] ffmpeg_opts Map of ffmpeg options key values pairs.
    //! @param[in] timeout     RTSP client io timeout. Doesn't mean the
    //!                        connection will be closed after this timeout but
    //!                        specifies the amount of time ffmpeg spends in io
    //!                        loop spinning, infinite timeout causes spinning
    //!                        forever if connection wasn't closed but no data
    //!                        was received.
    //! @param[in] in_streams  Input streams. Media formats source should use
    //!                        instead of auto-detection, this may decrease
    //!                        source start time and memory usage.
    //!                        Empty array causes avformat_find_stream_info()
    //!                        usage.
    rtsp_source(transport rtp_transport = transport::ASYNC_TCP,
                std::vector<Streamer::MediaType> media_types = {},
                std::map<std::string, std::string> ffmpeg_opts = {},
                std::chrono::seconds timeout = std::chrono::seconds(0),
                std::vector<Streamer::StreamInfo> in_streams = {})
        : ffmpeg::Source(in_streams),
          rtp_transport_ {rtp_transport},
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
        av_dict_set(&opts, "skip_idct", "any", AV_OPT_SEARCH_CHILDREN);

        av_dict_set(&opts, "rtsp_transport", __transport_to_ff(rtp_transport_),
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
            rtp_transport_ == transport::UDP
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
using rtsp_source_ptr = std::shared_ptr<rtsp_source>;
}  // namespace media
}  // namespace vxg

#endif