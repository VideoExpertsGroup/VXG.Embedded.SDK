#pragma once

#include "base_streamer.h"
#include "ffmpeg_common.h"

namespace vxg {
namespace media {
namespace ffmpeg {
//! @brief Base ffmpeg source class
class Source : public Streamer::ISource, public ffmpeg::common {
    vxg::logger::logger_ptr logger {vxg::logger::instance(name())};

public:
    Source(std::vector<Streamer::StreamInfo> suggested_input_streams = {});
    virtual ~Source();

    //! @brief Init ffmpeg source with specific ffmpeg options
    //!
    //! @param[in] url Url
    //! @param[in] opts ffmpeg options
    //! @param[in] fmt ffmpeg input format to prevent auto-detection.
    //!                ex.: "flv", "rtsp", "http" etc.
    //! @return true
    //! @return false
    bool init(std::string url, AVDictionary* opts, std::string fmt = "");
    //! @brief Init ffmpeg memory source with specific ffmpeg options
    //!
    //! @param[in] input_buffer Input memory buffer containing whole media.
    //! @param[in] opts ffmpeg options
    //! @param[in] fmt ffmpeg input format to prevent auto-detection.
    //!                ex.: "flv", "mp4", "http" etc.
    //! @return true
    //! @return false
    bool init(std::shared_ptr<std::vector<uint8_t>> input_buffer,
              AVDictionary* opts,
              std::string fmt);

    virtual bool init(std::string url = "");

    virtual void finit();
    virtual std::shared_ptr<Streamer::MediaFrame> pullFrame();
    virtual std::string name() { return "ffmpeg-source"; }
    virtual std::vector<Streamer::StreamInfo> negotiate();
    virtual void stop();

private:
    bool video_config_sent_;
    std::vector<uint8_t> sps_;
    std::vector<uint8_t> pps_;
    std::vector<uint8_t> sps_pps_;
};

}  // namespace ffmpeg
}  // namespace media
}  // namespace vxg
