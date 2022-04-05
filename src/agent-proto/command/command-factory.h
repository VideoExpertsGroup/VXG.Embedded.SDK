#ifndef __COMMAND_FACTORY_H
#define __COMMAND_FACTORY_H

#include <utils/logging.h>
#include <nlohmann/json.hpp>

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

class factory {
    vxg::logger::logger_ptr logger {};
    static constexpr const char* TAG = "agent-proto-command-factory";

public:
    static std::shared_ptr<base_command> parse(const json& j) {
        base_command::ptr command = nullptr;

        if (j.contains("cmd")) {
            std::string cmd = j["cmd"];

            if (cmd == REGISTER)
                command = std::make_shared<register_cmd>(j);
            else if (cmd == CAM_REGISTER)
                command = std::make_shared<cam_register>(j);
            else if (cmd == HELLO)
                command = std::make_shared<hello>(j);
            else if (cmd == CONFIGURE)
                command = std::shared_ptr<configure>(new configure(j));
            else if (cmd == GET_CAM_STATUS)
                command =
                    std::shared_ptr<get_cam_status>(new get_cam_status(j));
            else if (cmd == GET_SUPPORTED_STREAMS)
                command = std::shared_ptr<get_supported_streams>(
                    new get_supported_streams(j));
            else if (cmd == SUPPORTED_STREAMS)
                command = std::shared_ptr<supported_streams>(
                    new supported_streams(j));
            else if (cmd == CAM_HELLO)
                command = std::shared_ptr<cam_hello>(new cam_hello(j));
            else if (cmd == STREAM_START)
                command = std::shared_ptr<stream_start>(new stream_start(j));
            else if (cmd == STREAM_STOP)
                command = std::shared_ptr<stream_stop>(new stream_stop(j));
            else if (cmd == CAM_UPDATE_PREVIEW)
                command = std::shared_ptr<cam_update_preview>(
                    new cam_update_preview(j));
            else if (cmd == CAM_UPGRADE_FIRMWARE)
                command = std::shared_ptr<cam_upgrade_firmware>(
                    new cam_upgrade_firmware(j));
            else if (cmd == GET_AUDIO_DETECTION)
                command = std::shared_ptr<get_audio_detection>(
                    new get_audio_detection(j));
            else if (cmd == GET_CAM_AUDIO_CONF)
                command = std::shared_ptr<get_cam_audio_conf>(
                    new get_cam_audio_conf(j));
            else if (cmd == GET_CAM_EVENTS)
                command =
                    std::shared_ptr<get_cam_events>(new get_cam_events(j));
            else if (cmd == SET_CAM_EVENTS)
                command =
                    std::shared_ptr<set_cam_events>(new set_cam_events(j));
            else if (cmd == CAM_EVENTS_CONF)
                command =
                    std::shared_ptr<cam_events_conf>(new cam_events_conf(j));
            else if (cmd == CAM_EVENT)
                command = std::shared_ptr<cam_event>(new cam_event(j));
            else if (cmd == GET_CAM_VIDEO_CONF)
                command = std::shared_ptr<get_cam_video_conf>(
                    new get_cam_video_conf(j));
            else if (cmd == CAM_VIDEO_CONF)
                command =
                    std::shared_ptr<cam_video_conf>(new cam_video_conf(j));
            else if (cmd == DIRECT_UPLOAD_URL)
                command = std::shared_ptr<direct_upload_url>(
                    new direct_upload_url(j));
            else if (cmd == GET_DIRECT_UPLOAD_URL)
                command = std::shared_ptr<get_direct_upload_url>(
                    new get_direct_upload_url(j));
            else if (cmd == GET_MOTION_DETECTION)
                command = std::shared_ptr<get_motion_detection>(
                    new get_motion_detection(j));
            else if (cmd == MOTION_DETECTION_CONF)
                command = std::shared_ptr<motion_detection_conf>(
                    new motion_detection_conf(j));
            else if (cmd == GET_STREAM_BY_EVENT)
                command = std::shared_ptr<get_stream_by_event>(
                    new get_stream_by_event(j));
            else if (cmd == STREAM_BY_EVENT_CONF)
                command = std::shared_ptr<stream_by_event_conf>(
                    new stream_by_event_conf(j));
            else if (cmd == SET_STREAM_BY_EVENT)
                command = std::shared_ptr<set_stream_by_event>(
                    new set_stream_by_event(j));
            else if (cmd == GET_STREAM_CAPS)
                command =
                    std::shared_ptr<get_stream_caps>(new get_stream_caps(j));
            else if (cmd == STREAM_CAPS)
                command = std::shared_ptr<stream_caps>(new stream_caps(j));
            else if (cmd == GET_STREAM_CONFIG)
                command = std::shared_ptr<get_stream_config>(
                    new get_stream_config(j));
            else if (cmd == STREAM_CONFIG)
                command = std::shared_ptr<stream_config>(new stream_config(j));
            else if (cmd == SET_CAM_AUDIO_CONF)
                command = std::shared_ptr<set_cam_audio_conf>(
                    new set_cam_audio_conf(j));
            else if (cmd == CAM_AUDIO_CONF)
                command =
                    std::shared_ptr<cam_audio_conf>(new cam_audio_conf(j));
            else if (cmd == SET_CAM_VIDEO_CONF)
                command = std::shared_ptr<set_cam_video_conf>(
                    new set_cam_video_conf(j));
            else if (cmd == SET_MOTION_DETECTION)
                command = std::shared_ptr<set_motion_detection>(
                    new set_motion_detection(j));
            else if (cmd == SET_STREAM_CONFIG)
                command = std::shared_ptr<set_stream_config>(
                    new set_stream_config(j));
            else if (cmd == GET_PTZ_CONF)
                command = std::shared_ptr<get_ptz_conf>(new get_ptz_conf(j));
            else if (cmd == CAM_PTZ_CONF)
                command = std::shared_ptr<cam_ptz_conf>(new cam_ptz_conf(j));
            else if (cmd == CAM_PTZ)
                command = std::shared_ptr<cam_ptz>(new cam_ptz(j));
            else if (cmd == CAM_PTZ_PRESET)
                command =
                    std::shared_ptr<cam_ptz_preset>(new cam_ptz_preset(j));
            else if (cmd == CAM_PTZ_PRESET_CREATED)
                command = std::shared_ptr<cam_ptz_preset_created>(
                    new cam_ptz_preset_created(j));
            else if (cmd == RAW_MESSAGE)
                command = std::shared_ptr<raw_message>(new raw_message(j));
            else if (cmd == RAW_MESSAGE_CLIENT_CONNECTED)
                command = std::shared_ptr<raw_message_client_connected>(
                    new raw_message_client_connected(j));
            else if (cmd == RAW_MESSAGE_CLIENT_DISCONNECTED)
                command = std::shared_ptr<raw_message_client_disconnected>(
                    new raw_message_client_disconnected(j));
            else if (cmd == CAM_GET_LOG)
                command = std::shared_ptr<cam_get_log>(new cam_get_log(j));
            else if (cmd == GET_OSD_CONF)
                command = std::shared_ptr<get_osd_conf>(new get_osd_conf(j));
            else if (cmd == OSD_CONF)
                command = std::shared_ptr<osd_conf>(new osd_conf(j));
            else if (cmd == SET_OSD_CONF)
                command = std::shared_ptr<set_osd_conf>(new set_osd_conf(j));
            else if (cmd == SET_CAM_PARAMETER)
                command = std::shared_ptr<set_cam_parameter>(
                    new set_cam_parameter(j));
            else if (cmd == CAM_TRIGGER_EVENT)
                command = std::shared_ptr<cam_trigger_event>(
                    new cam_trigger_event(j));
            else if (cmd == BACKWARD_START)
                command =
                    std::shared_ptr<backward_start>(new backward_start(j));
            else if (cmd == BACKWARD_STOP)
                command = std::shared_ptr<backward_stop>(new backward_stop(j));
            else if (cmd == CAM_SET_CURRENT_WIFI)
                command = std::shared_ptr<cam_set_current_wifi>(
                    new cam_set_current_wifi(j));
            else if (cmd == CAM_LIST_WIFI)
                command = std::shared_ptr<cam_list_wifi>(new cam_list_wifi(j));
            else if (cmd == CAM_WIFI_LIST)
                command = std::shared_ptr<cam_wifi_list>(new cam_wifi_list(j));
            else if (cmd == BYE)
                command = std::shared_ptr<bye>(new bye(j));
            else if (cmd == REPORT_PROBLEM)
                command =
                    std::shared_ptr<report_problem>(new report_problem(j));
            else if (cmd == CAM_STATUS)
                command = std::shared_ptr<cam_status>(new cam_status(j));
            else if (cmd == AUDIO_FILE_PLAY)
                command =
                    std::shared_ptr<audio_file_play>(new audio_file_play(j));
            else if (cmd == GET_CAM_MEMORYCARD_TIMELINE)
                command = std::make_shared<get_cam_memorycard_timeline>(j);
            else if (cmd == CAM_MEMORYCARD_TIMELINE)
                command = std::make_shared<cam_memorycard_timeline>(j);
            else if (cmd == CAM_MEMORYCARD_SYNCHRONIZE)
                command = std::make_shared<cam_memorycard_synchronize>(j);
            else if (cmd == CAM_MEMORYCARD_SYNCHRONIZE_STATUS)
                command =
                    std::make_shared<cam_memorycard_synchronize_status>(j);
            else if (cmd == CAM_MEMORYCARD_SYNCHRONIZE_CANCEL)
                command =
                    std::make_shared<cam_memorycard_synchronize_cancel>(j);
            else if (cmd == CAM_MEMORYCARD_RECORDING)
                command = std::make_shared<cam_memorycard_recording>(j);
            else
                vxg::logger::instance(TAG)->error("Unknown command {}",
                                                  cmd.c_str());

            return command;
        }

        vxg::logger::instance(TAG)->error(
            "Received command doesn't contain cmd field");
        return nullptr;
    }

    static base_command::ptr reply(const base_command* orig_cmd,
                                   const std::string& cmd) {
        std::shared_ptr<base_command> command = nullptr;

        if (!cmd.empty() && cmd != UnsetString) {
            if (cmd == REGISTER)
                command = base_command::ptr(new register_cmd());
            else if (cmd == CAM_REGISTER)
                command = std::shared_ptr<register_cmd>(new register_cmd());
            else if (cmd == HELLO)
                command = std::shared_ptr<hello>(new hello());
            else if (cmd == CONFIGURE)
                command = std::shared_ptr<configure>(new configure());
            else if (cmd == GET_CAM_STATUS)
                command = std::shared_ptr<get_cam_status>(new get_cam_status());
            else if (cmd == GET_SUPPORTED_STREAMS)
                command = std::shared_ptr<get_supported_streams>(
                    new get_supported_streams());
            else if (cmd == SUPPORTED_STREAMS)
                command =
                    std::shared_ptr<supported_streams>(new supported_streams());
            else if (cmd == CAM_HELLO)
                command = std::shared_ptr<cam_hello>(new cam_hello());
            else if (cmd == STREAM_START)
                command = std::shared_ptr<stream_start>(new stream_start());
            else if (cmd == STREAM_STOP)
                command = std::shared_ptr<stream_stop>(new stream_stop());
            else if (cmd == CAM_UPDATE_PREVIEW)
                command = std::shared_ptr<cam_update_preview>(
                    new cam_update_preview());
            else if (cmd == CAM_UPGRADE_FIRMWARE)
                command = std::shared_ptr<cam_upgrade_firmware>(
                    new cam_upgrade_firmware());
            else if (cmd == GET_AUDIO_DETECTION)
                command = std::shared_ptr<get_audio_detection>(
                    new get_audio_detection());
            else if (cmd == GET_CAM_AUDIO_CONF)
                command = std::shared_ptr<get_cam_audio_conf>(
                    new get_cam_audio_conf());
            else if (cmd == GET_CAM_EVENTS)
                command = std::shared_ptr<get_cam_events>(new get_cam_events());
            else if (cmd == SET_CAM_EVENTS)
                command = std::shared_ptr<set_cam_events>(new set_cam_events());
            else if (cmd == CAM_EVENTS_CONF)
                command =
                    std::shared_ptr<cam_events_conf>(new cam_events_conf());
            else if (cmd == CAM_EVENT)
                command = std::shared_ptr<cam_event>(new cam_event());
            else if (cmd == GET_CAM_VIDEO_CONF)
                command = std::shared_ptr<get_cam_video_conf>(
                    new get_cam_video_conf());
            else if (cmd == CAM_VIDEO_CONF)
                command = std::shared_ptr<cam_video_conf>(new cam_video_conf());
            else if (cmd == DIRECT_UPLOAD_URL)
                command =
                    std::shared_ptr<direct_upload_url>(new direct_upload_url());
            else if (cmd == GET_DIRECT_UPLOAD_URL)
                command = std::shared_ptr<get_direct_upload_url>(
                    new get_direct_upload_url());
            else if (cmd == GET_MOTION_DETECTION)
                command = std::shared_ptr<get_motion_detection>(
                    new get_motion_detection());
            else if (cmd == MOTION_DETECTION_CONF)
                command = std::shared_ptr<motion_detection_conf>(
                    new motion_detection_conf());
            else if (cmd == GET_STREAM_BY_EVENT)
                command = std::shared_ptr<get_stream_by_event>(
                    new get_stream_by_event());
            else if (cmd == STREAM_BY_EVENT_CONF)
                command = std::shared_ptr<stream_by_event_conf>(
                    new stream_by_event_conf());
            else if (cmd == SET_STREAM_BY_EVENT)
                command = std::shared_ptr<set_stream_by_event>(
                    new set_stream_by_event());
            else if (cmd == GET_STREAM_CAPS)
                command =
                    std::shared_ptr<get_stream_caps>(new get_stream_caps());
            else if (cmd == STREAM_CAPS)
                command = std::shared_ptr<stream_caps>(new stream_caps());
            else if (cmd == GET_STREAM_CONFIG)
                command =
                    std::shared_ptr<get_stream_config>(new get_stream_config());
            else if (cmd == STREAM_CONFIG)
                command = std::shared_ptr<stream_config>(new stream_config());
            else if (cmd == SET_CAM_AUDIO_CONF)
                command = std::shared_ptr<set_cam_audio_conf>(
                    new set_cam_audio_conf());
            else if (cmd == CAM_AUDIO_CONF)
                command = std::shared_ptr<cam_audio_conf>(new cam_audio_conf());
            else if (cmd == SET_CAM_VIDEO_CONF)
                command = std::shared_ptr<set_cam_video_conf>(
                    new set_cam_video_conf());
            else if (cmd == SET_MOTION_DETECTION)
                command = std::shared_ptr<set_motion_detection>(
                    new set_motion_detection());
            else if (cmd == SET_STREAM_CONFIG)
                command =
                    std::shared_ptr<set_stream_config>(new set_stream_config());
            else if (cmd == GET_PTZ_CONF)
                command = std::shared_ptr<get_ptz_conf>(new get_ptz_conf());
            else if (cmd == CAM_PTZ_CONF)
                command = std::shared_ptr<cam_ptz_conf>(new cam_ptz_conf());
            else if (cmd == CAM_PTZ)
                command = std::shared_ptr<cam_ptz>(new cam_ptz());
            else if (cmd == CAM_PTZ_PRESET)
                command = std::shared_ptr<cam_ptz_preset>(new cam_ptz_preset());
            else if (cmd == CAM_PTZ_PRESET_CREATED)
                command = std::shared_ptr<cam_ptz_preset_created>(
                    new cam_ptz_preset_created());
            else if (cmd == RAW_MESSAGE)
                command = std::shared_ptr<raw_message>(new raw_message());
            else if (cmd == CAM_GET_LOG)
                command = std::shared_ptr<cam_get_log>(new cam_get_log());
            else if (cmd == GET_OSD_CONF)
                command = std::shared_ptr<get_osd_conf>(new get_osd_conf());
            else if (cmd == OSD_CONF)
                command = std::shared_ptr<osd_conf>(new osd_conf());
            else if (cmd == SET_OSD_CONF)
                command = std::shared_ptr<set_osd_conf>(new set_osd_conf());
            else if (cmd == SET_CAM_PARAMETER)
                command =
                    std::shared_ptr<set_cam_parameter>(new set_cam_parameter());
            else if (cmd == CAM_TRIGGER_EVENT)
                command =
                    std::shared_ptr<cam_trigger_event>(new cam_trigger_event());
            else if (cmd == BACKWARD_START)
                command = std::shared_ptr<backward_start>(new backward_start());
            else if (cmd == BACKWARD_STOP)
                command = std::shared_ptr<backward_stop>(new backward_stop());
            else if (cmd == CAM_SET_CURRENT_WIFI)
                command = std::shared_ptr<cam_set_current_wifi>(
                    new cam_set_current_wifi());
            else if (cmd == CAM_LIST_WIFI)
                command = std::shared_ptr<cam_list_wifi>(new cam_list_wifi());
            else if (cmd == CAM_WIFI_LIST)
                command = std::shared_ptr<cam_wifi_list>(new cam_wifi_list());
            else if (cmd == BYE)
                command = std::shared_ptr<bye>(new bye());
            else if (cmd == REPORT_PROBLEM)
                command = std::shared_ptr<report_problem>(new report_problem());
            else if (cmd == CAM_STATUS)
                command = std::shared_ptr<cam_status>(new cam_status());
            else if (cmd == AUDIO_FILE_PLAY)
                command =
                    std::shared_ptr<audio_file_play>(new audio_file_play());
            else if (cmd == GET_CAM_MEMORYCARD_TIMELINE)
                command = std::make_shared<get_cam_memorycard_timeline>();
            else if (cmd == CAM_MEMORYCARD_TIMELINE)
                command = std::make_shared<cam_memorycard_timeline>();
            else if (cmd == CAM_MEMORYCARD_SYNCHRONIZE)
                command = std::make_shared<cam_memorycard_synchronize>();
            else if (cmd == CAM_MEMORYCARD_SYNCHRONIZE_STATUS)
                command = std::make_shared<cam_memorycard_synchronize_status>();
            else if (cmd == CAM_MEMORYCARD_SYNCHRONIZE_CANCEL)
                command = std::make_shared<cam_memorycard_synchronize_cancel>();
            else if (cmd == CAM_MEMORYCARD_RECORDING)
                command = std::make_shared<cam_memorycard_recording>();
            else
                vxg::logger::instance(TAG)->error("Unknown command {}",
                                                  cmd.c_str());

            if (command) {
                command->cam_id = orig_cmd->cam_id;
                command->refid = orig_cmd->msgid;
                command->orig_cmd = orig_cmd->cmd;
            }

            return command;
        }

        vxg::logger::instance(TAG)->error(
            "Received command doesn't contain cmd field");
        return nullptr;
    }

    static command::done::ptr done(base_command* command,
                                   command::done_status status) {
        return command::done::ptr(new command::done(command, status));
    }

    static base_command::ptr create(const std::string& name) {
        json cmd = base_command(name);
        return parse(cmd);
    }
};
}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif