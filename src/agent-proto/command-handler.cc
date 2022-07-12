#include <functional>
#include <memory>

#include <net/websockets.h>
#include <utils/utils.h>

#include <agent-proto/command-handler.h>
#include <agent-proto/command/command-factory.h>
#include <agent-proto/command/command-list.h>
#include <agent-proto/command/utils.h>
#include <agent-proto/objects/caps.h>
#include <agent-proto/objects/config.h>

#include <config.h>

namespace vxg {
namespace cloud {

using websocket = transport::libwebsockets::websocket;
namespace agent {
namespace proto {

command_handler::command_handler(const agent::config& config,
                                 const proto::access_token& access_token,
                                 transport::worker::ptr transport)
    : transport_ {transport}, proto_config_ {config, access_token} {
    if (transport == nullptr)
        transport_ = std::make_shared<websocket>(
            std::bind(&command_handler::on_receive, this, placeholders::_1),
            config.allow_invalid_ssl_certs);
}

command_handler::~command_handler() {}

void command_handler::on_connected() {
    hello_received_ = false;
    registered_ = false;

    bye_reason_ = BR_INVALID;

    _reset_reconnect_timer();

    do_register();
}

void command_handler::on_error(void* unused, std::string msg) {
    _reset_reconnect_timer();

    hello_received_ = false;
    registered_ = false;

    on_closed(-1, BR_CONN_CLOSE);
}

void command_handler::on_disconnected() {
    _reset_reconnect_timer();

    hello_received_ = false;
    registered_ = false;

    if (bye_reason_ != command::bye_reason::BR_INVALID)
        on_closed(-1, bye_reason_);
    else
        on_closed(-1, BR_CONN_CLOSE);
}

void command_handler::_reset_reconnect_timer() {
    if (reconnect_timer_) {
        transport_->cancel_timed_cb(reconnect_timer_);
        reconnect_timer_ = nullptr;
    }
}

// Reconnect to websockets server in case of network error or bye message.
bool command_handler::try_connect() {
    return transport_->connect(proto_config_.buildUrl(proto::config::UT_PROTO));
}

bool command_handler::reconnect(int retry_timeout) {
    if (!reconnect_timer_) {
        transport_->disconnect();
        reconnect_timer_ = transport_->schedule_timed_cb(
            std::bind(&command_handler::try_connect, this), retry_timeout);
    } else {
        logger->warn("Reconnection is ongoing, skipping reconnect request");
    }
    return true;
}

json command_handler::handle_hello(hello* hello) {
    try {
        if (hello->ca != UnsetString)
            proto_config_.ca = hello->ca;
        if (hello->sid != UnsetString)
            proto_config_.sid = hello->sid;
        // For instances with old proto
        if (hello->upload_url != UnsetString)
            proto_config_.upload_url = hello->upload_url;
        // New proto, overrides upload_url field if presented
        if (hello->upload_uri != UnsetString)
            proto_config_.upload_url = hello->upload_uri;
        if (hello->media_server != UnsetString)
            proto_config_.media_server = hello->media_server;
        if (hello->connid != UnsetString)
            proto_config_.conn_id = hello->connid;

        do_cam_register();

        return json(*factory::done(hello, done_status::DS_OK));
    } catch (const std::exception& e) {
        logger->error("Can't handle command HELLO: {}", e.what());
        return json(*factory::done(hello, done_status::DS_MISSED_PARAM));
    }
}

json command_handler::handle_configure(configure* configure) {
    try {
        if (!__is_unset(configure->pwd))
            proto_config_.pwd = configure->pwd;

        if (!__is_unset(configure->uuid))
            proto_config_.uuid = configure->uuid;

        if (!__is_unset(configure->connid))
            proto_config_.conn_id = configure->connid;

        if (!__is_unset(configure->tz)) {
            proto_config_.timezone = configure->tz;
            on_set_timezone(proto_config_.timezone);
        }

        if (!__is_unset(configure->server))
            proto_config_.reconnect_server_address = configure->server;

        return json(*factory::done(configure, done_status::DS_OK));
    } catch (const std::exception& e) {
        logger->error("Can't handle command CONFIGURE: {}", e.what());
        return json(*factory::done(configure, done_status::DS_MISSED_PARAM));
    }
}

json command_handler::handle_get_cam_status(get_cam_status* getCamStatus) {
    cam_status::ptr camStatus = dynamic_pointer_cast<cam_status>(
        factory::reply(getCamStatus, CAM_STATUS));

    camStatus->ip = proto_config_.device_ipaddr;
    camStatus->activity = proto_config_.activity;
    camStatus->streaming = proto_config_.streaming;
    camStatus->status_led = false;

    return json(*camStatus);
}

json command_handler::handle_get_supported_streams(
    get_supported_streams* getSupportedStreams) {
    supported_streams::ptr supportedStreams =
        dynamic_pointer_cast<supported_streams>(
            factory::reply(getSupportedStreams, SUPPORTED_STREAMS));
    proto::supported_streams_config supportedStreamsConf;

    if (!on_get_supported_streams(supportedStreamsConf)) {
        return json(
            *factory::done(getSupportedStreams, done_status::DS_CM_ERROR));
    }

    *static_cast<proto::supported_streams_config*>(supportedStreams.get()) =
        supportedStreamsConf;

    return json(*supportedStreams);
}

json command_handler::handle_cam_hello(cam_hello* camHello) {
    base_command response;

    if (!__is_unset(camHello->cam_id))
        proto_config_.cam_id = camHello->camid();

    if (!__is_unset(camHello->media_uri))
        proto_config_.media_server = camHello->media_uri;

    if (!__is_unset(camHello->path))
        proto_config_.cam_path = camHello->path;

    proto_config_.activity = camHello->activity;

    on_set_activity(camHello->activity);

    hello_received_ = true;

    // We are good to send events here
    on_prepared();

    return json(*factory::done(camHello, done_status::DS_OK));
}

json command_handler::handle_bye(bye* bye) {
    bye_reason reason;
    long long retry;

    if (!__is_unset(bye->reason))
        reason = bye->reason;

    if (!__is_unset(bye->retry))
        retry = bye->retry;
    else
        retry = 0;

    bye_reason_ = reason;
    bye_retry_ = retry;
    hello_received_ = false;

    // No reply required according to ServerAPI_v4
    // Empty response
    return json();
}

json command_handler::handle_report_problem(report_problem* reportProblem) {
    std::string reason;

    if (!__is_unset(reportProblem->reason))
        reason = reportProblem->reason;

    logger->error("Cloud reported a problem: {}", reason);

    return json();
}

json command_handler::handle_stream_start(stream_start* streamStart) {
    proto::stream_reason reason = proto::SR_INVALID;
    std::string streamId;
    int publishSessionID = -1;

    if (!__is_unset(streamStart->stream_id))
        streamId = streamStart->stream_id;

    if (!__is_unset(streamStart->publish_session_id))
        publishSessionID = streamStart->publish_session_id;

    if (!__is_unset(streamStart->reason))
        reason = streamStart->reason;

    if (!on_stream_start(streamId, publishSessionID, reason))
        return json(*factory::done(streamStart, done_status::DS_CM_ERROR));

    return json(*factory::done(streamStart, done_status::DS_OK));
}

json command_handler::handle_stream_stop(stream_stop* streamStop) {
    proto::stream_reason reason = proto::SR_INVALID;
    std::string streamId;

    if (!__is_unset(streamStop->stream_id))
        streamId = streamStop->stream_id;

    if (!__is_unset(streamStop->reason))
        reason = streamStop->reason;

    if (!on_stream_stop(streamId, reason))
        return json(*factory::done(streamStop, done_status::DS_CM_ERROR));

    return json(*factory::done(streamStop, done_status::DS_OK));
}

json command_handler::handle_get_cam_events(get_cam_events* getCamEvents) {
    cam_events_conf::ptr camEventsConf = dynamic_pointer_cast<cam_events_conf>(
        factory::reply(getCamEvents, CAM_EVENTS_CONF));
    proto::events_config eventsConfig;

    if (!on_get_cam_events_config(eventsConfig))
        return json(*factory::done(getCamEvents, done_status::DS_CM_ERROR));

    *static_cast<proto::events_config*>(camEventsConf.get()) = eventsConfig;

    return json(*camEventsConf);
}

json command_handler::handle_set_cam_events(set_cam_events* setCamEvents) {
    proto::events_config eventsConfig =
        *(static_cast<proto::events_config*>(setCamEvents));

    if (!on_set_cam_events_config(eventsConfig))
        return json(*factory::done(setCamEvents, done_status::DS_CM_ERROR));

    return json(*factory::done(setCamEvents, done_status::DS_OK));
}

json command_handler::handle_direct_upload_url(
    direct_upload_url* directUploadUrl) {
    std::string url;
    json headers;

    if (__is_unset(directUploadUrl->status) ||
        __is_unset(directUploadUrl->url) ||
        __is_unset(directUploadUrl->headers) ||
        (directUploadUrl->status != "OK")) {
        logger->error("'direct_upload_url' status != OK");
        return json(
            *factory::done(directUploadUrl, done_status::DS_MISSED_PARAM));
    }

    on_direct_upload_url(*static_cast<direct_upload_url_base*>(directUploadUrl),
                         directUploadUrl->event_id, directUploadUrl->ref_id());

    for (auto& d : directUploadUrl->extra) {
        if (d.status == "OK") {
            on_direct_upload_url(d, directUploadUrl->event_id,
                                 directUploadUrl->ref_id());
        }
    }

    return json(*factory::done(directUploadUrl, done_status::DS_OK));
}

json command_handler::handle_get_motion_detection(
    get_motion_detection* getMotionDetection) {
    motion_detection_conf::ptr motionDetectionConf =
        dynamic_pointer_cast<motion_detection_conf>(
            factory::reply(getMotionDetection, MOTION_DETECTION_CONF));
    proto::motion_detection_config config;

    if (!on_get_motion_detection_config(config))
        return json(
            *factory::done(getMotionDetection, done_status::DS_SYSTEM_ERROR));

    *static_cast<proto::motion_detection_config*>(motionDetectionConf.get()) =
        config;

    return json(*motionDetectionConf);
}

json command_handler::handle_get_stream_caps(get_stream_caps* getStreamCaps) {
    proto::command::stream_caps::ptr streamCaps =
        dynamic_pointer_cast<proto::command::stream_caps>(
            factory::reply(getStreamCaps, STREAM_CAPS));
    proto::stream_caps caps;

    // Fill caps with requested video and audio es streams
    for (auto& vs : getStreamCaps->video_es) {
        proto::stream_caps::caps_video_object vcaps;
        vcaps.streams.push_back(vs);
        caps.caps_video.push_back(vcaps);
    }

    for (auto& as : getStreamCaps->audio_es) {
        proto::stream_caps::caps_audio_object acaps;
        acaps.streams.push_back(as);
        caps.caps_audio.push_back(acaps);
    }

    if (!on_get_stream_caps(caps))
        return json(*factory::done(getStreamCaps, done_status::DS_CM_ERROR));

    *static_cast<proto::stream_caps*>(streamCaps.get()) = caps;

    return json(*streamCaps);
}

json command_handler::handle_cam_update_preview(
    cam_update_preview* camUpdatePreview) {
    std::string url = proto_config_.buildUrl(proto::config::UT_UPLOAD_PREVIEW);

    if (!on_update_preview(url))
        return json(*factory::done(camUpdatePreview, done_status::DS_CM_ERROR));

    return json(*factory::done(camUpdatePreview, done_status::DS_OK));
}

json command_handler::handle_cam_upgrade_firmware(
    cam_upgrade_firmware* camUpgradeFirmware) {
    if (!on_cam_upgrade_firmware(camUpgradeFirmware->url))
        return json(
            *factory::done(camUpgradeFirmware, done_status::DS_CM_ERROR));

    return json(*factory::done(camUpgradeFirmware, done_status::DS_OK));
}

json command_handler::handle_get_audio_detection(
    get_audio_detection* getAudioDetection) {
    auto audioDetectionConfig = std::dynamic_pointer_cast<audio_detection_conf>(
        factory::reply(getAudioDetection, AUDIO_DETECTION_CONF));

    if (!on_get_audio_detection(
            *std::dynamic_pointer_cast<proto::audio_detection_config>(
                audioDetectionConfig)))
        return json(
            *factory::done(getAudioDetection, done_status::DS_NOT_SUPPORTED));

    return json(*audioDetectionConfig);
}

json command_handler::handle_set_audio_detection(
    set_audio_detection* setAudioDetection) {
    if (!on_set_audio_detection(*(setAudioDetection)))
        return json(
            *factory::done(setAudioDetection, done_status::DS_NOT_SUPPORTED));

    return json(*factory::done(setAudioDetection, done_status::DS_OK));
}

json command_handler::handle_get_stream_by_event(
    get_stream_by_event* getStreamByEvent) {
    stream_by_event_conf::ptr streamByEventConf =
        dynamic_pointer_cast<stream_by_event_conf>(
            factory::reply(getStreamByEvent, STREAM_BY_EVENT_CONF));
    proto::stream_by_event_config config;

    if (!on_get_stream_by_event(config)) {
        return json(*factory::done(getStreamByEvent, done_status::DS_CM_ERROR));
    }

    *static_cast<proto::stream_by_event_config*>(streamByEventConf.get()) =
        config;

    return json(*streamByEventConf);
}

json command_handler::handle_set_stream_by_event(
    set_stream_by_event* setStreamByEvent) {
    if (!on_set_stream_by_event(*setStreamByEvent))
        return json(*factory::done(setStreamByEvent, done_status::DS_CM_ERROR));

    return json(*factory::done(setStreamByEvent, done_status::DS_OK));
}

json command_handler::handle_get_stream_config(
    get_stream_config* getStreamConfig) {
    command::stream_config::ptr streamConfig =
        dynamic_pointer_cast<command::stream_config>(
            factory::reply(getStreamConfig, STREAM_CONFIG));
    proto::stream_config config;

    for (auto& video_stream : getStreamConfig->video_es) {
        proto::video_stream_config video_config;
        video_config.stream = video_stream;
        config.video.push_back(video_config);
    }

    for (auto& audio_stream : getStreamConfig->audio_es) {
        proto::audio_stream_config audio_config;
        audio_config.stream = audio_stream;
        config.audio.push_back(audio_config);
    }

    if (on_get_stream_config(config)) {
        *static_cast<proto::stream_config*>(streamConfig.get()) = config;
    } else {
        return json(
            *factory::done(getStreamConfig, done_status::DS_SYSTEM_ERROR));
    }

    return json(*streamConfig);
}

json command_handler::handle_set_stream_config(
    set_stream_config* setStreamConfig) {
    if (!on_set_stream_config(*(setStreamConfig)))
        return json(
            *factory::done(setStreamConfig, done_status::DS_INVALID_PARAM));

    return json(*factory::done(setStreamConfig, done_status::DS_OK));
}

json command_handler::handle_get_cam_audio_conf(
    get_cam_audio_conf* getCamAudioConf) {
    cam_audio_conf::ptr camAudioConf = dynamic_pointer_cast<cam_audio_conf>(
        factory::reply(getCamAudioConf, CAM_AUDIO_CONF));
    proto::audio_config audioConfig;

    if (!on_get_cam_audio_config(audioConfig))
        return json(
            *factory::done(getCamAudioConf, done_status::DS_SYSTEM_ERROR));

    *static_cast<proto::audio_config*>(camAudioConf.get()) = audioConfig;
    return json(*camAudioConf);
}

json command_handler::handle_set_cam_audio_conf(
    set_cam_audio_conf* setCamAudioConf) {
    if (!on_set_cam_audio_config(*setCamAudioConf))
        return json(
            *factory::done(setCamAudioConf, done_status::DS_INVALID_PARAM));

    return json(*factory::done(setCamAudioConf, done_status::DS_OK));
}

json command_handler::handle_get_cam_video_conf(
    get_cam_video_conf* getCamVideoConf) {
    cam_video_conf::ptr camVideoConf = dynamic_pointer_cast<cam_video_conf>(
        factory::reply(getCamVideoConf, CAM_VIDEO_CONF));
    proto::video_config videoConfig;

    if (!on_get_cam_video_config(videoConfig))
        return json(
            *factory::done(getCamVideoConf, done_status::DS_SYSTEM_ERROR));

    *static_cast<proto::video_config*>(camVideoConf.get()) = videoConfig;

    return json(*camVideoConf);
}

json command_handler::handle_set_cam_video_conf(
    set_cam_video_conf* setCamVideoConf) {
    if (!on_set_cam_video_config(*setCamVideoConf))
        return json(
            *factory::done(setCamVideoConf, done_status::DS_INVALID_PARAM));

    return json(*factory::done(setCamVideoConf, done_status::DS_OK));
}

json command_handler::handle_set_motion_detection(
    set_motion_detection* setMotionDetection) {
    if (!on_set_motion_detection_config(*setMotionDetection))
        return json(
            *factory::done(setMotionDetection, done_status::DS_INVALID_PARAM));

    return json(*factory::done(setMotionDetection, done_status::DS_OK));
}

json command_handler::handle_get_ptz_conf(get_ptz_conf* getPtzConf) {
    cam_ptz_conf::ptr camPTZConf = dynamic_pointer_cast<cam_ptz_conf>(
        factory::reply(getPtzConf, CAM_PTZ_CONF));
    proto::ptz_config ptzConfig;

    if (!on_get_ptz_config(ptzConfig))
        return json(*factory::done(getPtzConf, done_status::DS_NOT_SUPPORTED));

    *static_cast<proto::ptz_config*>(camPTZConf.get()) = ptzConfig;

    return json(*camPTZConf);
}

json command_handler::handle_cam_ptz(cam_ptz* camPtz) {
    if (!on_cam_ptz(*camPtz))
        return json(*factory::done(camPtz, done_status::DS_SYSTEM_ERROR));

    return json(*factory::done(camPtz, done_status::DS_OK));
}

json command_handler::handle_cam_ptz_preset(cam_ptz_preset* camPTZPreset) {
    if (!on_cam_ptz_preset(*camPTZPreset))
        return json(*factory::done(camPTZPreset, done_status::DS_SYSTEM_ERROR));

    // Callee must fill the token if preset was successfully created
    if (camPTZPreset->action == PA_CREATE && !__is_unset(camPTZPreset->token)) {
        cam_ptz_preset_created::ptr preset_created =
            dynamic_pointer_cast<cam_ptz_preset_created>(
                factory::reply(camPTZPreset, CAM_PTZ_PRESET_CREATED));

        preset_created->token = camPTZPreset->token;

        return json(*preset_created);
    }

    return json(*factory::done(camPTZPreset, done_status::DS_OK));
}

json command_handler::handle_raw_message(raw_message* rawMessage) {
    std::string message = rawMessage->message;

    if (!__is_unset(message) &&
        on_raw_message(rawMessage->client_id, message)) {
        if (!message.empty()) {
            // Response with
            raw_message reply = json(*factory::create(RAW_MESSAGE));
            reply.client_id = rawMessage->client_id;
            reply.message = message;
            send_command(json(reply));
        }
    }

    return json(*factory::done(rawMessage, done_status::DS_OK));
}

json command_handler::handle_raw_message_client_connected(
    raw_message_client_connected* rawMessage) {
    // TODO:
    return json(*factory::done(rawMessage, done_status::DS_OK));
}

json command_handler::handle_raw_message_client_disconnected(
    raw_message_client_disconnected* rawMessage) {
    // TODO:
    return json(*factory::done(rawMessage, done_status::DS_OK));
}

json command_handler::handle_cam_get_log(cam_get_log* camGetLog) {
    if (!on_get_log())
        return json(*factory::done(camGetLog, done_status::DS_CM_ERROR));

    return json(*factory::done(camGetLog, done_status::DS_OK));
}

json command_handler::handle_get_osd_conf(get_osd_conf* getOsdConf) {
    osd_conf::ptr osdConf =
        dynamic_pointer_cast<osd_conf>(factory::reply(getOsdConf, OSD_CONF));
    proto::osd_config osdConfig;

    if (!on_get_osd_config(osdConfig))
        return json(*factory::done(getOsdConf, done_status::DS_CM_ERROR));

    *static_cast<proto::osd_config*>(osdConf.get()) = osdConfig;

    return json(*osdConf);
}

json command_handler::handle_set_osd_conf(set_osd_conf* setOsdConf) {
    if (!on_set_osd_config(*setOsdConf))
        return json(*factory::done(setOsdConf, done_status::DS_INVALID_PARAM));

    return json(*factory::done(setOsdConf, done_status::DS_OK));
}

json command_handler::handle_set_cam_parameter(
    set_cam_parameter* setCamParameter) {
    // TODO: use setCamParameter->status_led

    on_set_activity(setCamParameter->activity);

    return json(*factory::done(setCamParameter, done_status::DS_OK));
}

json command_handler::handle_cam_trigger_event(
    cam_trigger_event* camTriggerEvent) {
    std::string event;
    json meta;
    cloud::time time;

    if (!__is_unset(camTriggerEvent->event))
        event = camTriggerEvent->event;
    if (!__is_unset(camTriggerEvent->time))
        time = utils::time::from_double(camTriggerEvent->time);
    if (!__is_unset(camTriggerEvent->meta))
        meta = camTriggerEvent->meta;

    if (event.empty() || !on_trigger_event(event, meta, time)) {
        return json(
            *factory::done(camTriggerEvent, done_status::DS_INVALID_PARAM));
    }

    return json(*factory::done(camTriggerEvent, done_status::DS_OK));
}

json command_handler::handle_backward_start(backward_start* backwardStart) {
    if (__is_unset(backwardStart->url))
        return json(
            *factory::done(backwardStart, done_status::DS_MISSED_PARAM));

    if (!on_start_backward(backwardStart->url))
        return json(
            *factory::done(backwardStart, done_status::DS_SYSTEM_ERROR));

    return json(*factory::done(backwardStart, done_status::DS_OK));
}

json command_handler::handle_backward_stop(backward_stop* backwardStop) {
    on_stop_backward(backwardStop->url);

    return json(*factory::done(backwardStop, done_status::DS_OK));
}

json command_handler::handle_cam_set_current_wifi(
    cam_set_current_wifi* camSetCurrentWifi) {
    if (!on_set_wifi_config(*camSetCurrentWifi))
        json(*factory::done(camSetCurrentWifi, done_status::DS_SYSTEM_ERROR));

    return json(*factory::done(camSetCurrentWifi, done_status::DS_OK));
}

json command_handler::handle_cam_list_wifi(cam_list_wifi* camListWifi) {
    cam_wifi_list::ptr wifiList = dynamic_pointer_cast<cam_wifi_list>(
        factory::reply(camListWifi, CAM_WIFI_LIST));
    proto::wifi_config config;

    if (!on_get_wifi_config(config))
        return json(*factory::done(camListWifi, done_status::DS_SYSTEM_ERROR));

    *static_cast<proto::wifi_config*>(wifiList.get()) = config;

    return json(*wifiList);
}

json command_handler::handle_audio_file_play(audio_file_play* audioFilePlay) {
    if (__is_unset(audioFilePlay->url))
        return json(
            *factory::done(audioFilePlay, done_status::DS_MISSED_PARAM));

    if (!on_audio_file_play(audioFilePlay->url))
        return json(
            *factory::done(audioFilePlay, done_status::DS_SYSTEM_ERROR));

    return json(*factory::done(audioFilePlay, done_status::DS_OK));
}

json command_handler::handle_get_cam_memorycard_timeline(
    get_cam_memorycard_timeline* get_timeline) {
    cam_memorycard_timeline::ptr timeline =
        dynamic_pointer_cast<cam_memorycard_timeline>(
            factory::reply(get_timeline, CAM_MEMORYCARD_TIMELINE));

    timeline->request_id = get_timeline->request_id;
    timeline->start = get_timeline->start;
    timeline->end = get_timeline->end;
    if (!on_get_cam_memorycard_timeline(*timeline))
        return json(*factory::done(get_timeline, done_status::DS_CM_ERROR));

    return json(*timeline);
}

json command_handler::handle_cam_memorycard_synchronize(
    cam_memorycard_synchronize* synchronize) {
    cam_memorycard_synchronize_status::ptr status =
        dynamic_pointer_cast<cam_memorycard_synchronize_status>(
            factory::reply(synchronize, CAM_MEMORYCARD_SYNCHRONIZE_STATUS));

    status->request_id = synchronize->request_id;

    for (auto& sync_requestid_to_cancel : synchronize->cancel_requests) {
        on_cam_memorycard_synchronize_cancel(sync_requestid_to_cancel);
    }

    if (!on_cam_memorycard_synchronize(
            *status, utils::time::from_iso_packed(synchronize->start),
            utils::time::from_iso_packed(synchronize->end)))
        return json(*factory::done(synchronize, done_status::DS_CM_ERROR));
    else
        status->status = proto::MCSS_PENDING;

    return json(*status);
}

json command_handler::handle_cam_memorycard_synchronize_cancel(
    cam_memorycard_synchronize_cancel* cancel) {
    cam_memorycard_synchronize_status::ptr status =
        dynamic_pointer_cast<cam_memorycard_synchronize_status>(
            factory::reply(cancel, CAM_MEMORYCARD_SYNCHRONIZE_STATUS));

    status->request_id = cancel->request_id;
    status->status = proto::MCSS_CANCELED;

    // Always success even we have nothing to cancel
    on_cam_memorycard_synchronize_cancel(cancel->request_id);

    return json(*factory::done(cancel, done_status::DS_OK));
}

json command_handler::handle_cam_memorycard_recording(
    cam_memorycard_recording* recording) {
    on_cam_memorycard_recording(recording->stream_id, recording->enabled);

    return json(*factory::done(recording, done_status::DS_OK));
}

bool command_handler::do_register() {
    register_cmd::ptr regCommand =
        dynamic_pointer_cast<register_cmd>(factory::create(REGISTER));
    register_cmd* command = regCommand.get();

    on_get_timezone(proto_config_.timezone);

    command->ver = proto_config_.agent_version;
    command->tz = proto_config_.timezone;
    command->vendor = proto_config_.device_vendor;

    if (!proto_config_.pwd.empty())
        command->pwd = proto_config_.pwd;

    if (!proto_config_.sid.empty())
        command->prev_sid = proto_config_.sid;

    if (!proto_config_.access_token_.token.empty())
        command->reg_token = proto_config_.access_token_.token;

    if (!transport_->is_secure())
        command->media_protocols.push_back("rtmp");
    else
        command->media_protocols.push_back("rtmps");

    logger->info("register: {}", json(*regCommand.get()).dump());

    return send_command(*command);
}

bool command_handler::do_cam_register() {
    if (!registered_) {
        cam_register::ptr camRegCommand =
            dynamic_pointer_cast<cam_register>(factory::create(CAM_REGISTER));
        cam_register* command = camRegCommand.get();

        command->ip = proto_config_.device_ipaddr;
        command->uuid = proto_config_.uuid;
        command->brand = proto_config_.device_brand;
        command->model = proto_config_.device_model;
        command->sn = proto_config_.device_serial;
        command->raw_messaging = proto_config_.raw_messaging;
        command->version = proto_config_.device_fw_version;
        command->type = proto_config_.device_type;

        on_registered(proto_config_.sid);

        return (registered_ = send_command(json(*command)));
    }

    return true;
}

bool command_handler::do_bye(std::string reason) {
    bye byeCommand;
    byeCommand.reason = json(reason);
    return send_command(&byeCommand);
}

void command_handler::_flush_command_acks() {
    for (auto ack = acks_.begin(); ack != acks_.end();) {
        // Disable timeout callback
        if (ack->second.timeout_cb_)
            transport_->cancel_timed_cb(ack->second.timeout_cb_);
        // Timed out callback, we didn't receive ack from the Cloud
        if (ack->second.ack_cb_)
            ack->second.ack_cb_(true, nullptr, ack->second.userdata_);
    }
}

void command_handler::_handle_command_ack(base_command::ptr command) {
    // Check if command refid is in the list of the acknowledge notifications
    // i.e. command is a reply to command sent with send_command_with_ack()
    if (acks_.count(command->refid) > 0) {
        auto ack = acks_[command->refid];

        // Disable timeout callback
        if (ack.timeout_cb_)
            transport_->cancel_timed_cb(ack.timeout_cb_);

        // Call ack notification with timeout flag set to false
        if (ack.ack_cb_)
            ack.ack_cb_(false, command, ack.userdata_);

        acks_.erase(command->refid);
    }
}

void command_handler::handleMessage(transport::Message& message) {
    base_command::ptr command = factory::parse(message);
    json responseCommand = nullptr;

    logger->info("C <= S: {}", message.dump());

    if (command == nullptr)
        return;

    if (!__is_unset(command->camid()) && proto_config_.cam_id &&
        command->camid() != proto_config_.cam_id) {
        logger->error("base_command's cam_id != cam_id from cam_hello");
        responseCommand =
            json(*factory::done(command.get(), done_status::DS_CM_ERROR));
        goto end;
    }

    _handle_command_ack(command);

    if (command->name() == HELLO)
        responseCommand = handle_hello(static_cast<hello*>(command.get()));
    else if (command->name() == CONFIGURE)
        responseCommand =
            handle_configure(static_cast<configure*>(command.get()));
    else if (command->name() == GET_CAM_STATUS)
        responseCommand =
            handle_get_cam_status(static_cast<get_cam_status*>(command.get()));
    else if (command->name() == GET_SUPPORTED_STREAMS)
        responseCommand = handle_get_supported_streams(
            static_cast<get_supported_streams*>(command.get()));
    else if (command->name() == CAM_HELLO)
        responseCommand =
            handle_cam_hello(static_cast<cam_hello*>(command.get()));
    else if (command->name() == STREAM_START)
        responseCommand =
            handle_stream_start(static_cast<stream_start*>(command.get()));
    else if (command->name() == STREAM_STOP)
        responseCommand =
            handle_stream_stop(static_cast<stream_stop*>(command.get()));
    else if (command->name() == CAM_UPDATE_PREVIEW)
        responseCommand = handle_cam_update_preview(
            static_cast<cam_update_preview*>(command.get()));
    else if (command->name() == CAM_UPGRADE_FIRMWARE)
        responseCommand = handle_cam_upgrade_firmware(
            static_cast<cam_upgrade_firmware*>(command.get()));
    else if (command->name() == GET_AUDIO_DETECTION)
        responseCommand = handle_get_audio_detection(
            static_cast<get_audio_detection*>(command.get()));
    else if (command->name() == SET_AUDIO_DETECTION)
        responseCommand = handle_set_audio_detection(
            static_cast<set_audio_detection*>(command.get()));
    else if (command->name() == GET_CAM_AUDIO_CONF)
        responseCommand = handle_get_cam_audio_conf(
            static_cast<get_cam_audio_conf*>(command.get()));
    else if (command->name() == GET_CAM_EVENTS)
        responseCommand =
            handle_get_cam_events(static_cast<get_cam_events*>(command.get()));
    else if (command->name() == GET_CAM_VIDEO_CONF)
        responseCommand = handle_get_cam_video_conf(
            static_cast<get_cam_video_conf*>(command.get()));
    else if (command->name() == DIRECT_UPLOAD_URL)
        responseCommand = handle_direct_upload_url(
            static_cast<direct_upload_url*>(command.get()));
    else if (command->name() == GET_MOTION_DETECTION)
        responseCommand = handle_get_motion_detection(
            static_cast<get_motion_detection*>(command.get()));
    else if (command->name() == GET_STREAM_BY_EVENT)
        responseCommand = handle_get_stream_by_event(
            static_cast<get_stream_by_event*>(command.get()));
    else if (command->name() == SET_STREAM_BY_EVENT)
        responseCommand = handle_set_stream_by_event(
            static_cast<set_stream_by_event*>(command.get()));
    else if (command->name() == GET_STREAM_CAPS)
        responseCommand = handle_get_stream_caps(
            static_cast<get_stream_caps*>(command.get()));
    else if (command->name() == GET_STREAM_CONFIG)
        responseCommand = handle_get_stream_config(
            static_cast<get_stream_config*>(command.get()));
    else if (command->name() == SET_CAM_AUDIO_CONF)
        responseCommand = handle_set_cam_audio_conf(
            static_cast<set_cam_audio_conf*>(command.get()));
    else if (command->name() == SET_CAM_EVENTS)
        responseCommand =
            handle_set_cam_events(static_cast<set_cam_events*>(command.get()));
    else if (command->name() == SET_CAM_VIDEO_CONF)
        responseCommand = handle_set_cam_video_conf(
            static_cast<set_cam_video_conf*>(command.get()));
    else if (command->name() == SET_MOTION_DETECTION)
        responseCommand = handle_set_motion_detection(
            static_cast<set_motion_detection*>(command.get()));
    else if (command->name() == SET_STREAM_CONFIG)
        responseCommand = handle_set_stream_config(
            static_cast<set_stream_config*>(command.get()));
    else if (command->name() == GET_PTZ_CONF)
        responseCommand =
            handle_get_ptz_conf(static_cast<get_ptz_conf*>(command.get()));
    else if (command->name() == CAM_PTZ)
        responseCommand = handle_cam_ptz(static_cast<cam_ptz*>(command.get()));
    else if (command->name() == CAM_PTZ_PRESET)
        responseCommand =
            handle_cam_ptz_preset(static_cast<cam_ptz_preset*>(command.get()));
    else if (command->name() == RAW_MESSAGE)
        responseCommand =
            handle_raw_message(static_cast<raw_message*>(command.get()));
    else if (command->name() == RAW_MESSAGE_CLIENT_CONNECTED)
        responseCommand = handle_raw_message_client_connected(
            static_cast<raw_message_client_connected*>(command.get()));
    else if (command->name() == RAW_MESSAGE_CLIENT_DISCONNECTED)
        responseCommand = handle_raw_message_client_disconnected(
            static_cast<raw_message_client_disconnected*>(command.get()));
    else if (command->name() == CAM_GET_LOG)
        responseCommand =
            handle_cam_get_log(static_cast<cam_get_log*>(command.get()));
    else if (command->name() == GET_OSD_CONF)
        responseCommand =
            handle_get_osd_conf(static_cast<get_osd_conf*>(command.get()));
    else if (command->name() == SET_OSD_CONF)
        responseCommand =
            handle_set_osd_conf(static_cast<set_osd_conf*>(command.get()));
    else if (command->name() == SET_CAM_PARAMETER)
        responseCommand = handle_set_cam_parameter(
            static_cast<set_cam_parameter*>(command.get()));
    else if (command->name() == CAM_TRIGGER_EVENT)
        responseCommand = handle_cam_trigger_event(
            static_cast<cam_trigger_event*>(command.get()));
    else if (command->name() == BACKWARD_START)
        responseCommand =
            handle_backward_start(static_cast<backward_start*>(command.get()));
    else if (command->name() == BACKWARD_STOP)
        responseCommand =
            handle_backward_stop(static_cast<backward_stop*>(command.get()));
    else if (command->name() == CAM_SET_CURRENT_WIFI)
        responseCommand = handle_cam_set_current_wifi(
            static_cast<cam_set_current_wifi*>(command.get()));
    else if (command->name() == CAM_LIST_WIFI)
        responseCommand =
            handle_cam_list_wifi(static_cast<cam_list_wifi*>(command.get()));
    else if (command->name() == BYE)
        /* No reply required according to ServerAPI_v4 */
        handle_bye(static_cast<bye*>(command.get()));
    else if (command->name() == REPORT_PROBLEM)
        /* No reply required according to ServerAPI_v4 */
        handle_report_problem(static_cast<report_problem*>(command.get()));
    else if (command->name() == AUDIO_FILE_PLAY)
        /* No reply required according to ServerAPI_v4 */
        handle_audio_file_play(static_cast<audio_file_play*>(command.get()));
    else if (command->name() == GET_CAM_MEMORYCARD_TIMELINE)
        responseCommand = handle_get_cam_memorycard_timeline(
            static_cast<get_cam_memorycard_timeline*>(command.get()));
    else if (command->name() == CAM_MEMORYCARD_SYNCHRONIZE)
        responseCommand = handle_cam_memorycard_synchronize(
            static_cast<cam_memorycard_synchronize*>(command.get()));
    else if (command->name() == CAM_MEMORYCARD_SYNCHRONIZE_CANCEL)
        responseCommand = handle_cam_memorycard_synchronize_cancel(
            static_cast<cam_memorycard_synchronize_cancel*>(command.get()));
    else if (command->name() == CAM_MEMORYCARD_RECORDING)
        responseCommand = handle_cam_memorycard_recording(
            static_cast<cam_memorycard_recording*>(command.get()));
end:
    if (responseCommand.dump().c_str() != nullptr) {
        send_command(responseCommand);
    }
}

bool command_handler::send_command(base_command::ptr message) {
    logger->info("C => S: {}", json(*message).dump());
    return transport_->send(json(*message.get()).dump());
}

bool command_handler::send_command(const base_command* message) {
    logger->info("C => S: {}", json(*message).dump());

    return transport_->send(json(*message).dump());
}

bool command_handler::send_command(const json& message) {
    if (!message.empty()) {
        logger->info("C => S: {}", message.dump());
        return transport_->send(message.dump());
    } else
        return false;
}

bool command_handler::send_command_wait_ack(
    const json& message,
    ack_cb ack_cb,
    std::shared_ptr<void> ack_userdata,
    std::chrono::seconds ack_timeout = std::chrono::seconds(10)) {
    struct command_ack ack;

    // Acknowledge callback.
    // Will be called when the command with refid == message.msgid will be
    // received from the Cloud
    ack.ack_cb_ = ack_cb;

    // Opaque userdata, will be passed to ack cb
    ack.userdata_ = ack_userdata;

    // Charge timeout timer cb, timeout cb is ack_cb with timeout flag argument
    // set to true
    ack.timeout_cb_ = transport_->schedule_timed_cb(
        std::bind(ack_cb, true, nullptr, ack_userdata),
        std::chrono::duration_cast<std::chrono::milliseconds>(ack_timeout)
            .count());
    // Save ack struct into the acks pool
    acks_[message["msgid"].get<int>()] = std::move(ack);

    return send_command(message);
}

void command_handler::on_receive(transport::Message message) {
    handleMessage(message);
}

}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg