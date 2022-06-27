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

    std::string _derived_class_name() {
        return utils::gcc_abi::demangle(typeid(*this).name());
    }

public:
    typedef std::shared_ptr<rtsp_stream> ptr;

    //! @brief Construct a new rtsp stream object
    //!
    //! @param source_url RTSP url
    //! @param name Unique stream name
    //! @param rtp_transport_tcp true - RTP over TCP; false - RTP over UDP
    //! @param record_needs_source Indicates if stream needs source start
    //! before calling start_record() virtual method.
    rtsp_stream(std::string source_url,
                std::string name,
                bool rtp_transport_tcp = true,
                bool recorder_needs_source = false)
        : agent::media::stream(
              name,
              // RTSP source
              std::make_shared<vxg::media::rtsp_source>(rtp_transport_tcp,
                                                        _rtsp_medias_filter()),
              recorder_needs_source),
          source_uri_ {source_url} {}

    virtual ~rtsp_stream() {}

    virtual bool start(std::string not_used = "") {
        return media::stream::start(source_uri_);
    }

    bool get_supported_stream(proto::supported_stream_config& config) {
        logger->warn("{} default implementation should be overriden in {}",
                     __func__, _derived_class_name());

        config.id = cloud_name();
        config.video = "Video" + std::to_string(0);
        // config.audio = "Audio" + std::to_string(0);

        return true;
    }

    virtual bool get_stream_caps(proto::stream_caps& caps) override {
        logger->warn("{} not implemented, implement it in {}", __func__,
                     _derived_class_name());
        return false;
    }

    virtual bool get_stream_config(proto::stream_config& streamConfig) {
        logger->warn("{} not implemented, implement it in {}", __func__,
                     _derived_class_name());
        return false;
    }

    virtual bool set_stream_config(const proto::stream_config& streamConfig) {
        logger->warn("{} not implemented, implement it in {}", __func__,
                     _derived_class_name());
        return false;
    }

    virtual bool get_snapshot(
        proto::event_object::snapshot_info_object& snapshot) {
        logger->warn("{} not implemented, implement it in {}", __func__,
                     _derived_class_name());
        return false;
    }

    virtual std::vector<proto::video_clip_info>
    record_get_list(cloud::time begin, cloud::time end, bool align) {
        std::vector<proto::video_clip_info> empty_vector(0);
        logger->warn("{} not implemented, implement it in {}", __func__,
                     _derived_class_name());
        return empty_vector;
    }

    virtual proto::video_clip_info record_export(cloud::time begin,
                                                 cloud::time end) {
        proto::video_clip_info clip;
        logger->warn("{} not implemented, implement it in {}", __func__,
                     _derived_class_name());
        return clip;
    }

    virtual bool start_record() {
        logger->warn("{} not implemented, implement it in {}", __func__,
                     _derived_class_name());
        return false;
    }

    virtual bool stop_record() {
        logger->warn("{} not implemented, implement it in {}", __func__,
                     _derived_class_name());
        return true;
    }
};  // class rtsp_stream
}  // namespace media
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
