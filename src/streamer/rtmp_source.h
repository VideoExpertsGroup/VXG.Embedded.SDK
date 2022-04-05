#ifndef __RTMP_SOURCE_H
#define __RTMP_SOURCE_H

#include "ffmpeg_source.h"
#include "stream.h"

//! @file rtmp_source.h RTMP source

namespace vxg {
namespace media {

//! @brief RTMP source class
class rtmp_source : public vxg::media::ffmpeg::Source {
    vxg::logger::logger_ptr logger {vxg::logger::instance("media-rtmp-src")};
    bool rtp_over_tcp_ {true};

public:
    //! @brief Init source with url
    //!
    //! @param[in] url RTMP url
    //! @return true Success
    //! @return false Failed
    //!
    virtual bool init(std::string url) {
        AVDictionary* opts = NULL;
        av_dict_set(&opts, "stimeout", "10000000", AV_OPT_SEARCH_CHILDREN);
        av_dict_set(&opts, "analyzeduration", "500000", 0);
        if (rtp_over_tcp_)
            av_dict_set(&opts, "rtsp_transport", "tcp", AV_OPT_SEARCH_CHILDREN);

        if (!ffmpeg::Source::init(url, opts, "flv")) {
            logger->error("Unable to connect to stream");
            return false;
        }
        return true;
    }
};

}  // namespace media
}  // namespace vxg

#endif