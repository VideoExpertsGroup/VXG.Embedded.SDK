#pragma once

#include <agent-proto/command-handler.h>
#include <agent/manager.h>
#include <net/transport.h>
#include <net/websockets.h>

using namespace ::testing;
using namespace std;

using namespace vxg::cloud::transport;
using namespace vxg::cloud::agent;
using namespace vxg::cloud::agent::proto;

class MockCommandHandler : public command_handler {
public:
    MockCommandHandler(worker::ptr transport) : command_handler(transport) {}

    MOCK_METHOD(void, on_closed, (int, bye_reason), (override));
    MOCK_METHOD(void, on_prepared, (), (override));
    MOCK_METHOD(void, on_connected, ());
    MOCK_METHOD(void, on_disconnected, ());
    MOCK_METHOD(void, on_registered, (const std::string& sid), (override));

    MOCK_METHOD(bool,
                on_get_stream_config,
                (proto::stream_config&),
                (override));
    MOCK_METHOD(bool,
                on_set_stream_config,
                (const proto::stream_config&),
                (override));

    MOCK_METHOD(bool,
                on_get_motion_detection_config,
                (proto::motion_detection_config&),
                (override));
    MOCK_METHOD(bool,
                on_set_motion_detection_config,
                (const proto::motion_detection_config&),
                (override));

    MOCK_METHOD(bool,
                on_get_cam_video_config,
                (proto::video_config&),
                (override));
    MOCK_METHOD(bool,
                on_set_cam_video_config,
                (const proto::video_config&),
                (override));

    MOCK_METHOD(bool,
                on_get_cam_events_config,
                (proto::events_config&),
                (override));
    MOCK_METHOD(bool,
                on_set_cam_events_config,
                (const proto::events_config&),
                (override));

    MOCK_METHOD(bool,
                on_get_cam_audio_config,
                (proto::audio_config&),
                (override));
    MOCK_METHOD(bool,
                on_set_cam_audio_config,
                (const proto::audio_config&),
                (override));

    MOCK_METHOD(bool, on_get_ptz_config, (proto::ptz_config&), (override));
    MOCK_METHOD(bool, on_cam_ptz, (proto::ptz_command), (override));
    MOCK_METHOD(bool, on_cam_ptz_preset, (proto::ptz_preset&), (override));

    MOCK_METHOD(bool, on_get_osd_config, (proto::osd_config&), (override));
    MOCK_METHOD(bool,
                on_set_osd_config,
                (const proto::osd_config&),
                (override));

    MOCK_METHOD(bool, on_get_wifi_config, (proto::wifi_config&), (override));
    MOCK_METHOD(bool,
                on_set_wifi_config,
                (const proto::wifi_network&),
                (override));

    MOCK_METHOD(bool,
                on_stream_start,
                (const string&, int, proto::stream_reason),
                (override));
    MOCK_METHOD(bool,
                on_stream_stop,
                (const string&, proto::stream_reason),
                (override));

    MOCK_METHOD(bool, on_get_stream_caps, (proto::stream_caps&), (override));

    MOCK_METHOD(bool,
                on_get_supported_streams,
                (proto::supported_streams_config&),
                (override));

    MOCK_METHOD(bool,
                on_direct_upload_url,
                (const direct_upload_url_base& direct_upload,
                 int event_id,
                 int refid),
                (override));

    MOCK_METHOD(bool,
                on_trigger_event,
                (std::string event, json meta, vxg::cloud::time time),
                (override));

    MOCK_METHOD(bool, on_start_backward, (std::string&), (override));
    MOCK_METHOD(bool, on_stop_backward, (std::string&), (override));

    MOCK_METHOD(bool, on_set_log_enable, (bool), (override));

    MOCK_METHOD(bool, on_set_activity, (bool), (override));

    MOCK_METHOD(bool, on_raw_message, (string, string&), (override));

    MOCK_METHOD(bool,
                on_set_stream_by_event,
                (proto::stream_by_event_config conf),
                (override));

    MOCK_METHOD(bool,
                on_get_stream_by_event,
                (proto::stream_by_event_config & conf),
                (override));

    MOCK_METHOD(bool, on_update_preview, (std::string), (override));

    MOCK_METHOD(bool, on_get_log, (), (override));

    MOCK_METHOD(bool, on_get_timezone, (std::string&), (override));

    MOCK_METHOD(bool, on_set_timezone, (std::string), (override));

    MOCK_METHOD(bool, on_cam_upgrade_firmware, (std::string), (override));

    MOCK_METHOD(bool, on_audio_file_play, (std::string), (override));

    MOCK_METHOD(bool,
                on_get_cam_memorycard_timeline,
                (proto::command::cam_memorycard_timeline & timeline),
                (override));
    MOCK_METHOD(bool,
                on_cam_memorycard_synchronize,
                (proto::command::cam_memorycard_synchronize_status&,
                 vxg::cloud::time start,
                 vxg::cloud::time end),
                (override));
    MOCK_METHOD(bool,
                on_cam_memorycard_synchronize_cancel,
                (const std::string& request_id),
                (override));

    MOCK_METHOD(bool,
                on_cam_memorycard_recording,
                (const std::string& stream_id, bool),
                (override));

    MOCK_METHOD(bool,
                on_set_audio_detection,
                (const proto::audio_detection_config&),
                (override));
    MOCK_METHOD(bool,
                on_get_audio_detection,
                (proto::audio_detection_config&),
                (override));
};
