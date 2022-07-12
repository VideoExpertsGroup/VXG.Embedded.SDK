#pragma once

#include <functional>

#include <agent/stream.h>
#include <streamer/rtsp_source.h>

using namespace vxg::cloud::agent;
using namespace vxg::media;

namespace vxg {
namespace cloud {
namespace agent {
namespace media {

//! @brief Implementation of the media::stream with RTSP source and NIY stubs.
class rtsp_stream : public agent::media::stream {
    vxg::logger::logger_ptr logger {vxg::logger::instance("media-stream-rtsp")};
    std::string source_uri_;

    static std::vector<vxg::media::Streamer::MediaType> _rtsp_medias_filter() {
        std::vector<vxg::media::Streamer::MediaType> medias;
        medias.push_back(vxg::media::Streamer::VIDEO);
        medias.push_back(vxg::media::Streamer::AUDIO);
        return medias;
    }

public:
    typedef std::shared_ptr<rtsp_stream> ptr;
    //! @brief Construct a new rtsp stream object
    //!
    //! @param source_url RTSP url
    //! @param name Unique stream name
    //! @param rtsp_src external rtsp_source object pointer
    //! @param recorder_needs_source Indicates if stream needs source start
    //! before calling start_record() virtual method.
    rtsp_stream(const std::string& source_url,
                const std::string& name,
                vxg::media::rtsp_source_ptr rtsp_src =
                    std::make_shared<vxg::media::rtsp_source>(
                        rtsp_source::transport::ASYNC_TCP,
                        std::vector<vxg::media::Streamer::MediaType> {
                            vxg::media::Streamer::VIDEO,
                            vxg::media::Streamer::AUDIO}),
                bool recorder_needs_source = false)
        : agent::media::stream(name,
                               // RTSP source
                               rtsp_src,
                               recorder_needs_source),
          source_uri_ {source_url} {}

    virtual ~rtsp_stream() {}

    virtual bool start(std::string not_used = "") override {
        return vxg::media::stream::start(source_uri_);
    }
};  // class rtsp_stream
}  // namespace media
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
