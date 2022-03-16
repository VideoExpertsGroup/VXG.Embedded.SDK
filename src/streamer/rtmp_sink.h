#ifndef __RTMP_SINK_H
#define __RTMP_SINK_H

#include "ffmpeg_sink.h"
#include "stream.h"

//! @file rtmp_sink.h RTMP sink

namespace vxg {
namespace media {

//! @brief RTMP sink class
class rtmp_sink : public vxg::media::ffmpeg::Sink {
    std::function<void(vxg::media::Streamer::StreamError)> error_cb_;

public:
    //! @brief Construct a new rtmp sink object
    //!
    //! @param[in] cb error callback
    rtmp_sink(std::function<void(vxg::media::Streamer::StreamError)> cb)
        : error_cb_ {cb} {}

    //! @brief Overriden [init](@ref vxg::media::ffmpeg::Sink::init(std::string,
    //! std::string)) method with hidden output ffmpeg format.
    //!
    //! @param url RTMP url
    //! @return true On success
    //! @return false On failure
    virtual bool init(std::string url) override {
        return ffmpeg::Sink::init(url, "flv");
    }

    virtual void error(Streamer::StreamError stream_error) override {
        if (stream_error == Streamer::E_EOS)
            vxg::logger::instance("rtmp-sink")->info("EOS, stopping");
        else {
            vxg::logger::instance("rtmp-sink")->error("error, stopping");
            if (error_cb_)
                error_cb_(stream_error);
        }

        ffmpeg::Sink::error(Streamer::E_FATAL);
    }

    virtual std::string name() override { return "rtmp-sink"; }

    virtual bool droppable() override { return true; }

    //! @brief Override negotiate() for removing all data streams.
    //!
    //! This is required for preventing buffering inside the ffmpeg muxer,
    //! ffmpeg waits for at least one packet for each stream or 10 seconds
    //! by default before output next chunk, this leads to 10 seconds delay
    //! if data track was added to output muxing context but no actual data
    //! packets were received hence sparse streams like onvif metadata may
    //! significantly increase delay.
    //! @param[in] streams_info - list of streams descrtiptions.
    //! @return true
    //! @return false
    bool negotiate(std::vector<Streamer::StreamInfo> streams_info) {
        auto i = std::begin(streams_info);

        while (i != std::end(streams_info)) {
            if (i->type == Streamer::StreamInfo::ST_DATA)
                i = streams_info.erase(i);
            else
                ++i;
        }

        // Call original negotiate() from the base class.
        return ffmpeg::Sink::negotiate(streams_info);
    }
};

}  // namespace media
}  // namespace vxg
#endif