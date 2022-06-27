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
    rtmp_sink() {}

    //! @brief Overriden [init](@ref vxg::media::ffmpeg::Sink::init(std::string,
    //! std::string)) method with hidden output ffmpeg format.
    //!
    //! @param url RTMP url
    //! @return true On success
    //! @return false On failure
    virtual bool init(std::string url) override {
        return ffmpeg::Sink::init(url, "flv");
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
        AVDictionary* opts = nullptr;

        // 10 seconds timeout for ffmpeg nonblocking socket handling loop to
        // prevent infinite busy looping on stuck tcp connection
        av_dict_set(&opts, "rw_timeout", "10000000", AV_OPT_SEARCH_CHILDREN);
        set_ff_opts(opts);

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