#pragma once

#include <map>
#include <memory>
#include <regex>

#include <agent-proto/objects/config.h>
#include <streamer/rtmp_sink.h>
#include <streamer/stream.h>
#include <utils/utils.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace media {

//! @brief Cloud agent media stream callbacks abstract class
//! @details VXG Cloud proto callbacks related to media stream
class stream_callbacks {
    //! @private
    //! @brief Indicates if stream should start source before calling
    //! start_record() virtual method.
    bool record_needs_source_ {false};

public:
    //! @brief Construct a new stream callbacks object
    //!
    //! @param recorder_needs_source Indicates if stream needs source start
    stream_callbacks(bool recorder_needs_source)
        : record_needs_source_ {recorder_needs_source} {}
    virtual ~stream_callbacks() {}
    //! @brief Get the media stream caps
    //! @details video/audio elementary streams caps request
    //!          passes caps with names of the elementary streams for
    //!          which caps are required to be filled inside this method
    //!
    //! @param[out] caps
    //! @return true if \p caps valid
    //! @return false if \p caps is invalid
    //!
    virtual bool get_stream_caps(cloud::agent::proto::stream_caps& caps) = 0;

    //! @brief Get the supported stream description
    //!
    //! @param[out] supported_stream Stream supported by device
    //! @return true if \p supported_stream is valid
    //! @return false if \p supported_stream is not valid
    //!
    virtual bool get_supported_stream(
        cloud::agent::proto::supported_stream_config& supported_stream) = 0;

    //! @brief Get the stream config
    //!
    //! @param[in,out] config input \p config contains list of streams for which
    //!                configuration should be returned
    //! @return true if \p config is valid
    //! @return false if \p config is invalid
    //!
    virtual bool get_stream_config(
        cloud::agent::proto::stream_config& config) = 0;

    //! @brief Set the streams config
    //!
    //! @param[in] config input \p config contains list of streams for which
    //!                configuration should be set
    //! @return true if \p config successfully set
    //! @return false if \p config failed to set
    //!
    virtual bool set_stream_config(
        const cloud::agent::proto::stream_config& config) = 0;

    //! @brief Get the snapshot image of this media stream
    //!
    //! @param[out] snapshot snapshot object
    //! @return true if \p snapshot is valid
    //! @return false if \p snapshot is invalid
    //!
    virtual bool get_snapshot(
        cloud::agent::proto::event_object::snapshot_info_object& snapshot) = 0;

    //! @brief Should returns true if agent::manager should start stream source
    //!        before calling start_record()
    //!
    //! @return true agent::manager should start stream source
    //! @return false agent::manager may not start stream source
    virtual bool record_needs_source() { return record_needs_source_; }

    //! @brief Start recording of this media stream.
    //!        Called only if memory card is presented and can be used.
    //!
    //! @return true if recording started
    //! @return false if recording start failed
    //! @see agent::event_stream::on_get_memorycard_info
    virtual bool start_record() = 0;

    //! @brief Stop recording of this stream
    virtual void stop_record() = 0;

    //! @brief Get list of the recorded clips for specific time period
    //!
    //! @param[in] begin beginning of the time period
    //! @param[in] end ending of the time period
    //! @param[in] align Align returned records to key frames and begin/end.
    //! If true the implementation should align returned records to not include
    //! data with timestamps less than @p begin and greater than @p end. Also
    //! any returned record MUST start with key frame and the last frame of any
    //! not last record in the list MUST be the frame prior to key frame - first
    //! frame of the next record.
    //! @param[in] limit Max records number that may be returned.
    //! Value 0 means no limitation.
    //! @return std::vector<proto::video_clip_info>
    virtual std::vector<cloud::agent::proto::video_clip_info>
    record_get_list(cloud::time begin, cloud::time end, bool align = true) = 0;
    //, size_t limit = 0

    //!
    //! @brief Export recorded clip for specified time
    //!
    //! @param begin
    //! @param end
    //! @return proto::video_clip_info
    //!
    virtual cloud::agent::proto::video_clip_info record_export(
        cloud::time begin,
        cloud::time end) = 0;
};

class stream_callbacks_stub : public stream_callbacks {
    vxg::logger::logger_ptr logger {vxg::logger::instance("media-stream-stub")};

    std::string name_;

    inline std::string _derived_class_name() {
        return utils::gcc_abi::demangle(typeid(*this).name());
    }

public:
    //! @brief Construct a new stream callbacks stub object
    //!
    //! @param name Stream name used for get_supported_stream() stub
    //! @param record_needs_source
    stream_callbacks_stub(std::string name, bool record_needs_source)
        : stream_callbacks(record_needs_source), name_ {name} {}
    virtual ~stream_callbacks_stub() {}

    bool get_supported_stream(proto::supported_stream_config& config) override {
        logger->warn("{} stub implementation should be overriden in {}",
                     __func__, _derived_class_name());

        config.id = name_;
        config.video = "Video" + std::to_string(0);
        // config.audio = "Audio" + std::to_string(0);

        return true;
    }

    virtual bool get_stream_caps(proto::stream_caps& caps) override {
        logger->warn("{} not implemented, implement it in {}", __func__,
                     _derived_class_name());
        return false;
    }

    virtual bool get_stream_config(
        proto::stream_config& streamConfig) override {
        logger->warn("{} not implemented, implement it in {}", __func__,
                     _derived_class_name());
        return false;
    }

    virtual bool set_stream_config(
        const proto::stream_config& streamConfig) override {
        logger->warn("{} not implemented, implement it in {}", __func__,
                     _derived_class_name());
        return false;
    }

    virtual bool get_snapshot(
        proto::event_object::snapshot_info_object& snapshot) override {
        logger->warn("{} not implemented, implement it in {}", __func__,
                     _derived_class_name());
        return false;
    }

    virtual std::vector<proto::video_clip_info>
    record_get_list(cloud::time begin, cloud::time end, bool align) override {
        std::vector<proto::video_clip_info> empty_vector(0);
        logger->warn("{} not implemented, implement it in {}", __func__,
                     _derived_class_name());
        return empty_vector;
    }

    virtual proto::video_clip_info record_export(cloud::time begin,
                                                 cloud::time end) override {
        proto::video_clip_info clip;
        logger->warn("{} not implemented, implement it in {}", __func__,
                     _derived_class_name());
        return clip;
    }

    virtual bool start_record() override {
        logger->warn("{} not implemented, implement it in {}", __func__,
                     _derived_class_name());
        return false;
    }

    virtual void stop_record() override {
        logger->warn("{} not implemented, implement it in {}", __func__,
                     _derived_class_name());
    }
};

//! @brief Cloud agent media stream abstract class
//! @details vxg::media::stream derived class with VXG Cloud proto callbacks
class stream : public stream_callbacks_stub, public vxg::media::stream {
public:
    //! @brief std::shared_ptr to the base_stream
    typedef std::shared_ptr<stream> ptr;

    //! @brief Construct a new agent media stream object
    //!
    //! @param[in] name Unique stream name which will be used by the VXG Cloud
    //!                 API
    //! @param[in] source Source object pointer
    //! @param[in] recorder_needs_source Indicates if stream needs source start
    //! before calling start_record() virtual method.
    //! @param[in] sink Sink object pointer, rtmp_sink as default
    stream(std::string name,
           vxg::media::Streamer::ISource::ptr source,
           bool recorder_needs_source = false,
           vxg::media::Streamer::ISink::ptr sink =
               std::make_shared<vxg::media::rtmp_sink>())
        : stream_callbacks_stub(name, recorder_needs_source),
          vxg::media::stream(name, source, sink) {}
    virtual ~stream() {}
};
using stream_ptr = std::shared_ptr<stream>;
}  // namespace media
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
