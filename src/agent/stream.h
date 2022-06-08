#ifndef __AGENT_STREAM_H
#define __AGENT_STREAM_H

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
//!
//! @brief Cloud agent media stream abstract class
//! @details vxg::media::stream derived class with VXG Cloud proto callbacks
//!
class stream : public vxg::media::stream {
    //! @private
    //! @brief Indicates if stream should start source before calling
    //! start_record() virtual method.
    bool record_needs_source_ {false};

public:
    //! @brief std::shared_ptr to the base_stream
    typedef std::shared_ptr<stream> ptr;

    //! @brief Construct a new agent media stream object
    //!
    //! @param[in] name Unique stream name which will be used by the VXG Cloud
    //!                 API
    //! @param[in] source Source object pointer
    //! @param[in] sink_error_cb Callback which will be called on sink error
    //! @param[in] recorder_needs_source Indicates if stream needs source start
    //! before calling start_record() virtual method.
    stream(std::string name,
           vxg::media::Streamer::ISource::ptr source,
           std::function<void(vxg::media::Streamer::StreamError)> sink_error_cb,
           bool recorder_needs_source = false)
        : vxg::media::stream(
              name,
              source,
              std::make_shared<vxg::media::rtmp_sink>(sink_error_cb)),
          record_needs_source_ {recorder_needs_source} {}
    virtual ~stream() {}

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
    //!
    //! @return true Stopped
    //! @return false Failed to stop
    //!
    virtual bool stop_record() = 0;

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
}  // namespace media
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
