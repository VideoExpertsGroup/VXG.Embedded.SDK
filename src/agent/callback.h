
#ifndef __CALLBACK_H
#define __CALLBACK_H

#include <string>

#include <agent-proto/proto.h>
#include <agent/config.h>

namespace vxg {
namespace cloud {
namespace agent {

//!
//! @brief VXG Cloud manager common callbacks class
//!
class callback {
public:
    //! @brief std::unique_ptr to callback
    typedef std::unique_ptr<callback> ptr;
    //! \private
    virtual ~callback() {};

    //!
    //! @brief VXG Cloud Bye command callback.
    //!
    //! @param reason bye reason
    //!
    virtual void on_bye(proto::command::bye_reason reason) = 0;

    //! @brief Registration on the Cloud has passed callback.
    //!
    //! @param sid Cloud connection session id. Must be saved and provided via
    //! the agent::config.cm_register_sid before the next
    //! vxg::cloud::agent::manager::start(), otherwise the Cloud will block
    //! connection with CONN_CONFLICT for some period of time.
    virtual void on_registered(const std::string& sid) {
        vxg::logger::warn("{} not implemented", __func__);
    }

    //!
    //! @brief raw message callback
    //!
    //! @param[in] client_id unique id of the client, every raw messages session
    //!            uses the same unique client_id
    //! @param[in,out] data raw message payload from client, output value will
    //!               be sent to the client if return value is true
    //! @return true raw message handled and reply in the output \p data
    //!         argument should be sent to the client as reply
    //! @return false raw message handling failure, \p data output argument
    //!         should not be sent to client
    //!
    virtual bool on_raw_msg(std::string client_id, std::string& data) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    //!
    //! @brief Get logging data callback
    //! @details Cloud API provides the way to request log data using Cloud API
    //!
    //! @param log_data log data
    //! @return true on success
    //! @return false on failure
    virtual bool on_get_log(std::string& log_data) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    //!
    //! @brief Start backward audio stream
    //!
    //! @param url rtmp url for backward channel, device supports backward audio
    //!            if on_get_cam_audio_config() set
    //!            proto::audio_config.caps spkr to true
    //! @details Implementation should start rtmp client by its own, final
    //!          implementation is also responsible for the demuxing, decoding
    //!          and rendering of the audio stream.
    //! @return true on success
    //! @return false on failure
    virtual bool on_start_backward_audio(std::string url) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    //!
    //! @brief Stop backward audio
    //!
    //! @param url backward audio url which was used to start the backward
    //! channel
    //!
    virtual bool on_stop_backward_audio(std::string url) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    //!
    //! @brief Get video image config
    //!
    //! @param[out] config video image config
    //! @return true if get image config success
    //! @return false get image config failed
    virtual bool on_get_cam_video_config(proto::video_config& config) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    //!
    //! @brief Set video input config
    //!
    //! @param config video input config
    //! @return true Video image input config was successfully set
    //! @return false Failed to set video input image config
    //!
    virtual bool on_set_cam_video_config(const proto::video_config& config) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    //!
    //! @brief Get audio input configuration
    //!
    //! @param[out] config audio input config
    //! @return true get audio input configuration success
    //! @return false get audio input configuration failed
    //!
    virtual bool on_get_cam_audio_config(proto::audio_config& config) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    //!
    //! @brief Set audio input/output config
    //!
    //! @param config audio input/output config
    //! @return true applied
    //! @return false failed to set config
    //!
    virtual bool on_set_cam_audio_config(const proto::audio_config& config) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    //!
    //! @brief Get PTZ config
    //!
    //! @param[out] config ptz config
    //! @return true success
    //! @return false Get PTZ config failed
    //!
    virtual bool on_get_ptz_config(proto::ptz_config& config) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    //!
    //! @brief PTZ command
    //!
    //! @param[in] command ptz command
    //! @return true success
    //! @return false PTZ command failure
    //!
    virtual bool on_cam_ptz(proto::ptz_command& command) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    //! @brief PTZ preset command
    //!
    //! @param[in,out] preset_op ptz preset operation, if operation is
    //!                proto::PA_CREATE the callee should fill the token.
    //! @return true PTZ preset operation success
    //! @return false PTZ preset operation failure
    //!
    virtual bool on_cam_ptz_preset(proto::ptz_preset& preset_op) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    //!
    //! @brief Get OSD config
    //!
    //! @param[out] config OSD config
    //! @return true OSD config get success, \p config is valid
    //! @return false OSD config get failure, \p config should not be used
    //!
    virtual bool on_get_osd_config(proto::osd_config& config) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    //!
    //! @brief Set OSD config
    //!
    //! @param[in] config OSD config
    //! @return true OSD config was successfully set
    //! @return false failed to set OSD config
    //!
    virtual bool on_set_osd_config(const proto::osd_config& config) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    //!
    //! @brief Get WiFi config
    //!
    //! @param[out] config WiFi config
    //! @return true success
    //! @return false failed
    //!
    virtual bool on_get_wifi_config(proto::wifi_config& config) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    //!
    //! @brief Set WiFi config
    //!
    //! @param[in] config WiFi configuration
    //! @return true if \p config is valid
    //! @return false if \p config is invalid
    //!
    virtual bool on_set_wifi_config(const proto::wifi_network& config) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    // Motion Detection
    //!
    //! @brief Get motion detection configuration
    //!
    //! @param[out] config Motion detection config if return value is true
    //! @return true if \p config is valid
    //! @return false if failed to get motion detection config
    //!
    virtual bool on_get_motion_detection_config(
        proto::motion_detection_config& config) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    //!
    //! @brief Set motion detection config
    //!
    //! @param[in] config motion detection config
    //! @return true if \p config was successfully set
    //! @return false if failed to set \p config
    //!
    virtual bool on_set_motion_detection_config(
        const proto::motion_detection_config& config) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    //!
    //! @brief Get device timezone in IANA format
    //!
    //! @param[out] timezone name in IANA format
    //! @return true if \p timezone is valid
    //! @return false if \p timezone is not valid
    //!
    virtual bool on_get_timezone(std::string& timezone) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    //!
    //! @brief Set device timezone in IANA format
    //!
    //! @param[in] timezone timezone in IANA format
    //! @return true if timezone was successfully set
    //! @return false if timezone was not set
    //!
    virtual bool on_set_timezone(std::string timezone) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    //! @brief Get memory card information,
    //!        If this callback returned false or if @p info status not equal to
    //!        proto::MCS_NORMAL, the recording will not
    //!        be started, i.e. no agent::media::stream::record_start() will be
    //!        called.
    //!
    //! @param[out] info memorycard info
    //! @return true if \p info is valid
    //! @return false if \p info is not valid
    //!
    virtual bool on_get_memorycard_info(
        proto::event_object::memorycard_info_object& info) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    //! @brief Firmware upgrade.
    //! @param[in] firmware Firmware binary data.
    //! @return true if firmware upgrade was successful.
    //! @return false if firmware upgrade failed.
    virtual bool on_cam_upgrade_firmware(const std::string& firmware) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    //! @brief Audio file play.
    //! @param[in] audio_file Audio file binary data.
    //! @param[in] audio_file_format Audio file data format.
    //! @return true if firmware upgrade was successful.
    //! @return false if firmware upgrade failed.
    virtual bool on_audio_file_play(const std::string audio_file_data,
                                    const std::string filename) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    virtual bool on_trigger_event(proto::event_object& event) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    virtual bool on_set_audio_detection(
        const proto::audio_detection_config& conf) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    virtual bool on_get_audio_detection(proto::audio_detection_config& conf) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }
};
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif