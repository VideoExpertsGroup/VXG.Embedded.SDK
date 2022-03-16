#pragma once

#include <cxxabi.h>

#include <streamer/ffmpeg_sink.h>
#include <streamer/rtmp_source.h>
#include <streamer/stream.h>

namespace vxg {
namespace media {

using namespace vxg::cloud;

class http_sink : public ffmpeg::Sink {
public:
    virtual std::string name() { return "http-sink"; }

    virtual bool init(std::string url) override {
        // ffmpeg output format pcm_s8 for audio passthrough without muxing
        return ffmpeg::Sink::init(url, "mulaw");
    }

    // -channels 1 -ab 32000 -ar 8000 -ac 1 -c:a adpcm_g726le -content_type
    // audio/G726-32 -multiple_requests 1 -f g726le -channels 1 -ab 128k -ar
    //
    // 8000 -ac 1 -c:a pcm_mulaw -content_type audio/basic -chunked_post 0
    // -multiple_requests 1
    virtual bool negotiate(std::vector<Streamer::StreamInfo> info) {
        using namespace vxg::media::Streamer;
        AVDictionary* opts = nullptr;
        std::string content_type = "";
        std::vector<Streamer::StreamInfo> audio_info;

        for (auto& i : info) {
            if (i.type == StreamInfo::ST_AUDIO) {
                switch (i.audio.codec) {
                    case StreamInfo::AC_G711_U:
                        content_type = "audio/basic";
                        break;
                    case StreamInfo::AC_AAC:
                        content_type = "audio/aac";
                        break;
                    case StreamInfo::AC_OPUS:
                        content_type = "audio/opus";
                        break;
                    case StreamInfo::AC_LPCM:
                        content_type = "audio/lpcm";
                        break;
                    case StreamInfo::AC_G726:
                        content_type = "audio/G726-24";
                        break;
                    default:
                        vxg::logger::warn("No mime type for audio codec {}",
                                          i.audio.codec);
                        break;
                }
                // Use audio track only, otherwise ffmpeg thinks we want to mux
                audio_info.push_back(i);
            }
        }

        if (!content_type.empty())
            av_dict_set(&opts, "content_type", content_type.c_str(),
                        AV_OPT_SEARCH_CHILDREN);
        else
            return false;

        // Turns on 'keep-alive'
        av_dict_set(&opts, "multiple_requests", "1", AV_OPT_SEARCH_CHILDREN);
        // Turns off 'Chunked'
        av_dict_set(&opts, "chunked_post", "0", AV_OPT_SEARCH_CHILDREN);
        // Don't wait for 100-continue
        // av_dict_set(&opts, "send_expect_100", "0", AV_OPT_SEARCH_CHILDREN);
        // Auto-reconnect
        av_dict_set(&opts, "reconnect", "1", AV_OPT_SEARCH_CHILDREN);
        // Some implementations require big number as content length
        // av_dict_set(&opts, "headers", "Content-Length: 999999999",
        //             AV_OPT_SEARCH_CHILDREN);

        set_ff_opts(opts);

        return ffmpeg::Sink::negotiate(audio_info);
    }

    virtual void error(Streamer::StreamError e) override { stop(); }
};

class rtmp_http_stream : public vxg::media::stream {
    vxg::logger::logger_ptr logger;

public:
    typedef std::shared_ptr<stream> ptr;

    rtmp_http_stream(std::string name)
        : vxg::media::stream(name,
                             std::make_shared<vxg::media::rtmp_source>(),
                             std::make_shared<vxg::media::http_sink>()),
          logger {vxg::logger::instance("rtmp-http-stream")} {}
};  // class stream

}  // namespace media
}  // namespace vxg