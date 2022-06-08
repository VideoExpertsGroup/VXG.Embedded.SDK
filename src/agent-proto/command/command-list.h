#pragma once

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {
const char* const HELLO = "hello";
const char* const CAM_UPDATE_PREVIEW = "cam_update_preview";
const char* const CAM_UPGRADE_FIRMWARE = "cam_upgrade_firmware";
const char* const DONE = "done";
const char* const REGISTER = "register";
const char* const CONFIGURE = "configure";
const char* const BYE = "bye";
const char* const CAM_REGISTER = "cam_register";
const char* const CAM_HELLO = "cam_hello";
const char* const GET_CAM_STATUS = "get_cam_status";
const char* const CAM_STATUS = "cam_status";
const char* const GET_CAM_VIDEO_CONF = "get_cam_video_conf";
const char* const CAM_VIDEO_CONF = "cam_video_conf";
const char* const GET_CAM_AUDIO_CONF = "get_cam_audio_conf";
const char* const CAM_AUDIO_CONF = "cam_audio_conf";
const char* const GET_SUPPORTED_STREAMS = "get_supported_streams";
const char* const SUPPORTED_STREAMS = "supported_streams";
const char* const GET_CAM_EVENTS = "get_cam_events";
const char* const CAM_EVENTS_CONF = "cam_events_conf";
const char* const SET_CAM_EVENTS = "set_cam_events";
const char* const GET_STREAM_BY_EVENT = "get_stream_by_event";
const char* const SET_STREAM_BY_EVENT = "set_stream_by_event";
const char* const STREAM_BY_EVENT_CONF = "stream_by_event_conf";
const char* const SET_CAM_VIDEO_CONF = "set_cam_video_conf";
const char* const SET_CAM_AUDIO_CONF = "set_cam_audio_conf";
const char* const STREAM_START = "stream_start";
const char* const STREAM_STOP = "stream_stop";
const char* const GET_STREAM_CAPS = "get_stream_caps";
const char* const STREAM_CAPS = "stream_caps";
const char* const GET_STREAM_CONFIG = "get_stream_config";
const char* const SET_STREAM_CONFIG = "set_stream_config";
const char* const STREAM_CONFIG = "stream_config";
const char* const CAM_EVENT = "cam_event";
const char* const GET_MOTION_DETECTION = "get_motion_detection";
const char* const MOTION_DETECTION_CONF = "motion_detection_conf";
const char* const SET_MOTION_DETECTION = "set_motion_detection";
const char* const GET_AUDIO_DETECTION = "get_audio_detection";
const char* const AUDIO_DETECTION_CONF = "audio_detection_conf";
const char* const SET_AUDIO_DETECTION = "set_audio_detection";
const char* const GET_PTZ_CONF = "get_ptz_conf";
const char* const CAM_PTZ_CONF = "cam_ptz_conf";
const char* const CAM_PTZ = "cam_ptz";
const char* const CAM_PTZ_PRESET = "cam_ptz_preset";
const char* const CAM_PTZ_PRESET_CREATED = "cam_ptz_preset_created";
const char* const GET_CAM_MEMORYCARD_TIMELINE = "get_cam_memorycard_timeline";
const char* const CAM_MEMORYCARD_TIMELINE = "cam_memorycard_timeline";
const char* const CAM_MEMORYCARD_SYNCHRONIZE = "cam_memorycard_synchronize";
const char* const CAM_MEMORYCARD_SYNCHRONIZE_STATUS =
    "cam_memorycard_synchronize_status";
const char* const CAM_MEMORYCARD_SYNCHRONIZE_CANCEL =
    "cam_memorycard_synchronize_cancel";
const char* const CAM_MEMORYCARD_RECORDING = "cam_memorycard_recording";

const char* const RAW_MESSAGE = "raw_message";
const char* const RAW_MESSAGE_CLIENT_CONNECTED = "raw_message_client_connected";
const char* const RAW_MESSAGE_CLIENT_DISCONNECTED =
    "raw_message_client_disconnected";

//=>upload segments
const char* const GET_DIRECT_UPLOAD_URL = "get_direct_upload_url";  // CM =>
const char* const DIRECT_UPLOAD_URL = "direct_upload_url";          // <=SRV
const char* const CONFIRM_DIRECT_UPLOAD = "confirm_direct_upload";  // CM =>
//<=upload segments

const char* const CAM_GET_LOG = "cam_get_log";
const char* const GET_OSD_CONF = "get_osd_conf";
const char* const SET_OSD_CONF = "set_osd_conf";
const char* const OSD_CONF = "osd_conf";
const char* const SET_CAM_PARAMETER = "set_cam_parameter";
const char* const CAM_TRIGGER_EVENT = "cam_trigger_event";
const char* const BACKWARD_START = "backward_start";
const char* const BACKWARD_STOP = "backward_stop";

const char* const CAM_LIST_WIFI = "cam_list_wifi";
const char* const CAM_WIFI_LIST = "cam_wifi_list";
const char* const CAM_SET_CURRENT_WIFI = "cam_set_current_wifi";

const char* const REPORT_PROBLEM = "report_problem";

const char* const AUDIO_FILE_PLAY = "audio_file_play";

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg

// clang-format off
#include <agent-proto/objects/config.h>
#include <agent-proto/objects/caps.h>
#include <agent-proto/command/utils.h>
#include <agent-proto/command/unset-helper.h>
#include <agent-proto/command/command.h>
#include <agent-proto/command/backward-start.h>
#include <agent-proto/command/backward-stop.h>
#include <agent-proto/command/bye.h>
#include <agent-proto/command/cam-get-log.h>
#include <agent-proto/command/cam-hello.h>
#include <agent-proto/command/cam-list-wifi.h>
#include <agent-proto/command/cam-ptz.h>
#include <agent-proto/command/cam-register.h>
#include <agent-proto/command/cam-set-current-wifi.h>
#include <agent-proto/command/cam-trigger-event.h>
#include <agent-proto/command/cam-update-preview.h>
#include <agent-proto/command/cam-upgrade-firmware.h>
#include <agent-proto/command/cam_event.h>
#include <agent-proto/command/configure.h>
#include <agent-proto/command/direct-upload-url.h>
#include <agent-proto/command/audio-detection.h>
#include <agent-proto/command/get-cam-audio-conf.h>
#include <agent-proto/command/get-cam-events.h>
#include <agent-proto/command/get-cam-status.h>
#include <agent-proto/command/get-cam-video-conf.h>
#include <agent-proto/command/get-direct-upload-url.h>
#include <agent-proto/command/get-motion-detection.h>
#include <agent-proto/command/get-osdconf.h>
#include <agent-proto/command/get-ptz-conf.h>
#include <agent-proto/command/get-stream-by-event.h>
#include <agent-proto/command/get-stream-caps.h>
#include <agent-proto/command/get-stream-config.h>
#include <agent-proto/command/get-supported-streams.h>
#include <agent-proto/command/hello.h>
#include <agent-proto/command/raw-message.h>
#include <agent-proto/command/register.h>
#include <agent-proto/command/report-problem.h>
#include <agent-proto/command/set-cam-audio-conf.h>
#include <agent-proto/command/set-cam-event.h>
#include <agent-proto/command/set-cam-parameter.h>
#include <agent-proto/command/set-cam-video-conf.h>
#include <agent-proto/command/set-motion-detection.h>
#include <agent-proto/command/set-osdconf.h>
#include <agent-proto/command/set-stream-by-event.h>
#include <agent-proto/command/set-stream-config.h>
#include <agent-proto/command/stream-start.h>
#include <agent-proto/command/stream-stop.h>
#include <agent-proto/command/audio-file.h>
#include <agent-proto/command/cam-memorycard.h>

// Should be last header
#include <agent-proto/command/command-factory.h>
// clang-format on
