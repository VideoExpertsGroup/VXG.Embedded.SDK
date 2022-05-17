#ifndef __CAMERAMANAGERWEBSOCKET_H__
#define __CAMERAMANAGERWEBSOCKET_H__

#include <agent/manager-config.h>

#include <agent-proto/proto.h>

#include <net/transport.h>
#include <utils/logging.h>
#include <utils/utils.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {

using namespace command;

class command_handler {
    vxg::logger::logger_ptr logger = vxg::logger::instance("agent-proto");

protected:
    bool registered_ {false};
    command::bye_reason bye_reason_ {command::bye_reason::BR_INVALID};
    long long bye_retry_ {0};
    bool hello_received_ {false};

    struct command_ack {
        std::function<void(bool, command::base_command::ptr)> ack_cb_;
        transport::timed_cb_ptr timeout_cb_;
    };
    std::map<int, command_ack> acks_;

public:
    command_handler(transport::worker::ptr transport = nullptr);
    virtual ~command_handler();

    virtual void on_connected();
    virtual void on_disconnected();
    virtual void on_error(void* unused, std::string msg);
    virtual void on_receive(transport::Message data);

    void handleMessage(transport::Message& message);

protected:
    // CM => SRV (send)
    bool do_register();
    bool do_cam_register();
    bool do_bye(std::string reason);

    // SRV => CM (receive)
    json handle_hello(hello* hello);
    json handle_configure(configure* configure);
    json handle_get_cam_status(get_cam_status* getCamStatus);
    json handle_get_supported_streams(
        get_supported_streams* getSupportedStreams);
    json handle_cam_hello(cam_hello* camHello);
    json handle_bye(bye* bye);
    json handle_stream_start(stream_start* streamStart);
    json handle_stream_stop(stream_stop* streamStop);
    json handle_cam_update_preview(cam_update_preview* camUpdatePreview);
    json handle_cam_upgrade_firmware(cam_upgrade_firmware* camUpgradeFirmware);
    json handle_get_audio_detection(get_audio_detection* getAudioDetection);
    json handle_get_cam_audio_conf(get_cam_audio_conf* getCamAudioConf);
    json handle_get_cam_events(get_cam_events* getCamEvents);
    json handle_get_cam_video_conf(get_cam_video_conf* getCamVideoConf);
    json handle_direct_upload_url(direct_upload_url* directUploadUrl);
    json handle_get_motion_detection(get_motion_detection* getMotionDetection);
    json handle_get_stream_by_event(get_stream_by_event* command);
    json handle_set_stream_by_event(set_stream_by_event* setStreamByEvent);
    json handle_get_stream_caps(get_stream_caps* getStreamCaps);
    json handle_get_stream_config(get_stream_config* getStreamConfig);
    json handle_set_cam_audio_conf(set_cam_audio_conf* setCamAudioConf);
    json handle_report_problem(report_problem* reportProblem);
    json handle_set_cam_events(set_cam_events* setCamEvents);
    json handle_set_cam_video_conf(set_cam_video_conf* setCamVideoConf);
    json handle_set_motion_detection(set_motion_detection* setMotionDetection);
    json handle_set_stream_config(set_stream_config* setStreamConfig);
    json handle_get_ptz_conf(get_ptz_conf* getPtzConf);
    json handle_cam_ptz(cam_ptz* camPtz);
    json handle_cam_ptz_preset(cam_ptz_preset* camPTZPreset);
    json handle_raw_message(raw_message* rawMessage);
    json handle_raw_message_client_connected(
        raw_message_client_connected* rawMessage);
    json handle_raw_message_client_disconnected(
        raw_message_client_disconnected* rawMessage);
    json handle_cam_get_log(cam_get_log* camGetLog);
    json handle_get_osd_conf(get_osd_conf* getOsdConf);
    json handle_set_osd_conf(set_osd_conf* getOsdConf);
    json handle_set_cam_parameter(set_cam_parameter* setCamParameter);
    json handle_cam_trigger_event(cam_trigger_event* camTriggerEvent);
    json handle_backward_start(backward_start* backwardStart);
    json handle_backward_stop(backward_stop* backwardStop);
    json handle_cam_set_current_wifi(cam_set_current_wifi* camSetCurrentWifi);
    json handle_cam_list_wifi(cam_list_wifi* camListWifi);
    json handle_audio_file_play(audio_file_play* audioFilePlay);
    json handle_get_cam_memorycard_timeline(
        get_cam_memorycard_timeline* get_timeline);
    json handle_cam_memorycard_synchronize(
        cam_memorycard_synchronize* synchronize);
    json handle_cam_memorycard_synchronize_cancel(
        cam_memorycard_synchronize_cancel* cancel);
    json handle_cam_memorycard_recording(cam_memorycard_recording* recording);

protected:
    std::shared_ptr<transport::worker> transport_;
    bool auto_reconnect_ {true};
    transport::timed_cb_ptr reconnect_timer_ {nullptr};
    manager_config cm_config_;

    bool try_connect();
    bool reconnect(int retry_timeout);
    void _reset_reconnect_timer();

    bool send_command(base_command::ptr message);
    bool send_command(const json& message);
    bool send_command(const base_command* message);

    bool send_command_wait_ack(
        const json& message,
        std::function<void(bool timed_out, proto::command::base_command::ptr)>
            ack_cb,
        std::chrono::seconds ack_timeout);
    void _handle_command_ack(base_command::ptr command);
    void _flush_command_acks();

    virtual void on_closed(int error, bye_reason reason) = 0;
    virtual void on_prepared() = 0;

    virtual bool on_get_stream_config(proto::stream_config& config) = 0;
    virtual bool on_set_stream_config(const proto::stream_config& config) = 0;

    virtual bool on_get_motion_detection_config(
        proto::motion_detection_config& config) = 0;
    virtual bool on_set_motion_detection_config(
        const proto::motion_detection_config& config) = 0;

    virtual bool on_get_cam_video_config(proto::video_config& config) = 0;
    virtual bool on_set_cam_video_config(const proto::video_config& config) = 0;

    virtual bool on_get_cam_events_config(proto::events_config& config) = 0;
    virtual bool on_set_cam_events_config(
        const proto::events_config& config) = 0;

    virtual bool on_get_cam_audio_config(proto::audio_config& config) = 0;
    virtual bool on_set_cam_audio_config(const proto::audio_config& config) = 0;

    virtual bool on_get_ptz_config(proto::ptz_config& config) = 0;
    virtual bool on_cam_ptz(proto::ptz_command command) = 0;
    virtual bool on_cam_ptz_preset(proto::ptz_preset& preset) = 0;

    virtual bool on_get_osd_config(proto::osd_config& config) = 0;
    virtual bool on_set_osd_config(const proto::osd_config& config) = 0;

    virtual bool on_get_wifi_config(proto::wifi_config& config) = 0;
    virtual bool on_set_wifi_config(const proto::wifi_network& config) = 0;

    virtual bool on_stream_start(const std::string& streamId,
                                 int publishSessionID,
                                 proto::stream_reason reason) = 0;
    virtual bool on_stream_stop(const std::string& streamId,
                                proto::stream_reason reason) = 0;

    virtual bool on_get_stream_caps(proto::stream_caps& caps) = 0;

    virtual bool on_get_supported_streams(
        proto::supported_streams_config& supportedStreamsConfig) = 0;

    virtual bool on_direct_upload_url(
        const proto::command::direct_upload_url_base& direct_upload,
        int event_id,
        int ref_id) = 0;
    virtual bool on_get_timezone(std::string& timezone) = 0;
    virtual bool on_set_timezone(std::string timezone) = 0;

    // SD recording synchronization
    virtual bool on_get_cam_memorycard_timeline(
        proto::command::cam_memorycard_timeline& timeline) = 0;
    virtual bool on_cam_memorycard_synchronize(
        proto::command::cam_memorycard_synchronize_status& synchronize_status,
        vxg::cloud::time start,
        vxg::cloud::time end) = 0;
    virtual bool on_cam_memorycard_synchronize_cancel(
        const std::string& request_id) = 0;
    virtual bool on_cam_memorycard_recording(const std::string& stream_id,
                                             bool enabled) = 0;

    virtual bool on_trigger_event(std::string event,
                                  json meta,
                                  cloud::time time) = 0;

    // Two-way audio backward channel
    virtual bool on_audio_file_play(std::string url) = 0;
    virtual bool on_start_backward(std::string& url) = 0;
    virtual bool on_stop_backward(std::string& url) = 0;

    virtual bool on_raw_message(std::string client_id, std::string& data) = 0;

    virtual bool on_set_stream_by_event(proto::stream_by_event_config conf) = 0;
    virtual bool on_get_stream_by_event(
        proto::stream_by_event_config& conf) = 0;

    virtual bool on_update_preview(std::string url) = 0;

    virtual bool on_cam_upgrade_firmware(std::string url) = 0;

    virtual bool on_get_log() = 0;

    // FIXME: all following commands should be refactored
    virtual bool on_set_log_enable(bool bEnable) = 0;
    virtual bool on_set_activity(bool bEnable) = 0;

    virtual void on_registered(const std::string& sid) = 0;
};
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif  //__CAMERAMANAGERWEBSOCKET_H__