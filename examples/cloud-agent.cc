#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>

#include <agent/manager.h>
#include <agent/rtsp-stream.h>

#include <utils/logging.h>
#include <utils/profile.h>
#include <utils/properties.h>

#include <config.h>

using namespace vxg::cloud;
using namespace vxg::cloud::agent;

std::string STREAM_MAIN_URL;
std::string CAMERA_LOGIN;
std::string CAMERA_PASSWORD;
std::string CLOUD_TOKEN;

int bTerminate = 0;
auto logger = vxg::logger::instance("cloud-agent");

static void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        fprintf(stderr, "\nSIGTERM received\n\n");
        bTerminate = 1;
    }
}

class agent_callback : public agent::callback {
public:
    // Error with cloud connection occured
    void on_bye(int error) { logger->info("on_bye %d", error); }

    // Handler for the raw messages from the server
    bool on_raw_msg(std::string cid, std::string& data) { return false; }

    // Server provides url to upload log file using http POST
    void on_get_log(std::string url) {}

    // Should return current time from the server
    time_t on_get_cloud_time() { return 0; }

    // Switching from LOGLEVEL_ERROR to LOGLEVEL_DEBUG and vice versa
    void set_log_enable(bool bEnable) {}

    virtual bool on_get_log(std::string& log_data) {
        std::vector<std::string> log_files;
        std::string log_file = "";
        log_files.push_back(log_file);

        for (auto& log : log_files) {
            std::ifstream log_stream(log, std::ios::in | std::ios::binary);
            if (!log_stream.fail()) {
                log_data.insert(log_data.end(),
                                (std::istreambuf_iterator<char>(log_stream)),
                                std::istreambuf_iterator<char>());
                log_stream.close();
            }
        }

        return true;
    }

    // Get camera timezone
    bool on_get_timezone(std::string& time_zone_str) {
        time_zone_str = "UTC";
        return true;
    }

    // Set camera timezone
    bool on_set_timezone(std::string time_zone) { return false; }

    // Called when Trigger event received from the server
    bool on_trigger_event(void* inst, string evt, string meta) { return false; }

    // Application should receive rtmp audio stream from this url and play it on
    // the camera
    bool on_start_backward_audio(string url) {
        logger->info("%s", __FUNCTION__);
        return false;
    }
    // Application should stops playback backward audio on the camera
    bool on_stop_backward_audio(string url) {
        logger->info("%s", __FUNCTION__);
        return false;
    }

    bool on_get_stream_config(proto::stream_config& config) {
        logger->info("%s", "Get stream config");

        // Only one video and audio stream supported so far
        proto::video_stream_config videoConf;
        videoConf.stream = "Vid";
        videoConf.format = proto::video_format::VF_H264;
        videoConf.fps = 15;
        videoConf.gop = 15;
        videoConf.horz = 640;
        videoConf.horz = 480;
        videoConf.brt = 500;
        videoConf.quality = 0;
        config.video.push_back(videoConf);

        proto::audio_stream_config audioConf;
        audioConf.stream = "Aud";
        audioConf.format = proto::audio_format::AF_AAC;
        audioConf.brt = 320;
        audioConf.srt = 48.0;
        config.audio.push_back(audioConf);

        return true;
    }

    bool on_set_stream_config(const proto::stream_config& config) {
        logger->info("Set stream config: %s", json(config).dump().c_str());

        return true;
    }

    bool on_get_motion_detection_config(
        proto::motion_detection_config& config) {
        proto::motion_region region;
        region.region = "reg_0";
        region.sensitivity = 33;
        region.map = utils::motion::map::pack("0101");
        region.enabled = true;
        config.regions.push_back(region);

        config.columns = 2;
        config.rows = 2;

        config.caps.region_shape = proto::motion_region_shape::MR_RECTANGLE;
        config.caps.max_regions = 2;
        config.caps.sensitivity = proto::motion_sensitivity::MS_FRAME;

        return true;
    }

    bool on_set_motion_detection_config(
        const proto::motion_detection_config& config) {
        logger->info("Cloud set motion detection %s",
                     json(config).dump().c_str());
        if (!config.regions.empty())
            logger->info("Cloud set motion detection region %s",
                         config.regions[0].region.c_str());

        return true;
    }

    bool on_get_cam_video_config(proto::video_config& config) {
        proto::video_caps caps;
        // Everything is off in caps by default, enabling contrast only
        caps.contrast = true;
        config.caps = caps;

        return true;
    }

    bool on_set_cam_video_config(const proto::video_config& config) {
        logger->info("Set video image config: %s", json(config).dump().c_str());
        // Only variables specified as supported in caps from
        // on_get_cam_video_config() should be used here

        std::this_thread::sleep_for(std::chrono::seconds(5));

        return true;
    }

    bool on_get_cam_events_config(proto::events_config& eventsConfig) {
        proto::event_type eventsList[] = {proto::ET_MOTION, proto::ET_WIFI,
                                          proto::ET_CUSTOM};
        const std::string customEventName = "custom";

        eventsConfig.enabled = true;
        for (const auto& eventType : eventsList) {
            proto::event_config eventConfig;

            eventConfig.event = eventType;
            if (eventType == proto::ET_CUSTOM)
                eventConfig.custom_event_name = customEventName;

            eventConfig.snapshot = eventType != proto::ET_WIFI;
            eventConfig.period = 0;
            eventConfig.active = true;
            eventConfig.stream = false;

            eventConfig.caps.stream = eventType != proto::ET_WIFI;
            eventConfig.caps.snapshot = eventType != proto::ET_WIFI;
            eventConfig.caps.trigger = false;
            eventConfig.caps.periodic = false;

            eventsConfig.events.push_back(eventConfig);
        }

        return true;
    }

    bool on_set_cam_events_config(const proto::events_config& config) {
        logger->info("Set events config: %s", json(config).dump().c_str());

        return true;
    }

    bool on_get_cam_audio_config(proto::audio_config& config) {
        // caps
        config.caps.echo_cancel.push_back(proto::M_OFF);
        config.caps.echo_cancel.push_back(proto::M_ON);
        // camera has speaker
        config.caps.spkr = true;
        // camera has microphone
        config.caps.mic = true;
        // camera support backward in AAC format
        config.caps.backward = true;
        config.caps.backward_formats.push_back(proto::audio_format::AF_AAC);

        // current settings
        config.spkr_mute = false;
        config.spkr_vol = 100;
        config.mic_mute = true;
        config.mic_gain = 100;
        config.echo_cancel = proto::M_OFF;

        return true;
    }

    bool on_set_cam_audio_config(const proto::audio_config& config) {
        logger->info("Set audio config: %s", json(config).dump().c_str());
        return true;
    }

    bool on_get_ptz_config(proto::ptz_config& config) {
        // Camera supports zoom only
        config.actions.push_back(proto::A_ZOOM_IN);
        config.actions.push_back(proto::A_ZOOM_OUT);

        // 2 presets are supported
        config.maximum_number_of_presets = 2;

        // here we should store presets created by server in onCamPtz()
        config.presets.clear();

        return true;
    }

    bool on_cam_ptz(proto::ptz_command& command) {
        logger->info("PTZ action %s from server",
                     std::to_string(command.action).c_str());

        return true;
    }

    bool on_get_osd_config(proto::osd_config& config) {
        // caps
        config.caps.date = true;
        config.caps.date_format.push_back("DD-MM-YYYY");
        config.caps.date_format.push_back("DD/MM/YYYY");

        config.caps.time = true;
        config.caps.time_format.push_back(proto::TF_12H);
        config.caps.time_format.push_back(proto::TF_24H);

        config.caps.system_id = true;
        config.caps.system_id_text = false;
        // rest of the caps will be treated as not supported

        // current state of device, all bool values are false by default
        config.date = true;
        config.date_format = "DD/MM/YYYY";
        config.time = true;
        config.time_format = proto::TF_12H;

        return true;
    }

    bool on_set_osd_config(const proto::osd_config& config) {
        logger->info("OSD settings: {}", json(config).dump());

        return true;
    }

    bool on_get_wifi_config(proto::wifi_config& config) {
        proto::wifi_network netOpen, netSecure;

        netOpen.encryption = proto::WFE_OPEN;
        netOpen.encryption_caps.push_back(proto::WFE_OPEN);
        netOpen.encryption_caps.push_back(proto::WFE_WEP);
        netOpen.mac = "00:11:22:33:44:55";
        netOpen.signal = 99;
        netOpen.ssid = "WIFI-OPEN";

        netSecure.encryption = proto::WFE_WPA2;
        netSecure.encryption_caps.push_back(proto::WFE_WPA);
        netSecure.encryption_caps.push_back(proto::WFE_WPA2);
        netSecure.mac = "00:11:22:33:44:66";
        netSecure.signal = 55;
        netSecure.ssid = "WIFI-TOPSECRET";

        config.networks.push_back(netOpen);
        config.networks.push_back(netSecure);

        return true;
    }

    bool on_set_wifi_config(const proto::wifi_network& config) {
        logger->info("Set current WiFi to {}", json(config).dump());

        return true;
    }

    bool on_get_memorycard_info(
        agent::proto::event_object::memorycard_info_object& info) {
        return false;
    }
};

bool init_agent_params() {
    CLOUD_TOKEN = Properties::Get("cloud_token");
    CAMERA_LOGIN = Properties::Get("login");
    CAMERA_PASSWORD = Properties::Get("password");
    STREAM_MAIN_URL = "rtsp://" + CAMERA_LOGIN + ":" + CAMERA_PASSWORD +
                      "@localhost/live1s1.sdp";

    if (std::getenv("CLOUD_TOKEN")) {
        CLOUD_TOKEN = std::getenv("CLOUD_TOKEN");
        logger->info("Override cloud token from the env variable");
    }

    if (std::getenv("STREAM_MAIN_URL")) {
        STREAM_MAIN_URL = std::getenv("STREAM_MAIN_URL");
        logger->info("Override main stream url from the env variable");
    }

    if (CLOUD_TOKEN.empty()) {
        logger->error("Properties files {} has no 'cloud_token'",
                      Properties::GetPropsFile().c_str());
        return false;
    }

    if (CAMERA_LOGIN.empty()) {
        logger->error("Properties files {} has no 'login'",
                      Properties::GetPropsFile().c_str());
        return false;
    }

    if (CAMERA_LOGIN.empty()) {
        logger->error("Properties files {} has no 'login'",
                      Properties::GetPropsFile().c_str());
        return false;
    }

    if (STREAM_MAIN_URL.empty()) {
        logger->error("Properties files {} has no 'stream_main_url'",
                      Properties::GetPropsFile().c_str());
        return false;
    }

    logger->info("Agent property['cloud_token']: {}", CLOUD_TOKEN.c_str());
    logger->info("Agent property['login']: {}", CAMERA_LOGIN.c_str());
    logger->info("Agent property['password']: {}", CAMERA_PASSWORD.c_str());

    return true;
}

class agent_callback_minimal : public agent::callback {
public:
    agent_callback_minimal() {}

    virtual void on_bye(proto::bye_reason reason) {
        vxg::logger::error("Error {}", json(reason).dump());
    }
};

int main(int argc, char** argv) {
    using namespace vxg::cloud::agent::media;
    Properties::SetPropsFile(PROPS_FILE_PATH);
    std::vector<agent::media::stream::ptr> streams;
    proto::access_token::ptr access_token;
    auto cb = std::make_unique<agent_callback_minimal>();
    manager::ptr agent;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, signal_handler);

    vxg::logger::info("VXG Cloud Agent Library Version: {}",
                      vxg::cloud::agent::version());

    init_agent_params();

    streams.push_back(
        std::make_shared<media::rtsp_stream>(STREAM_MAIN_URL, "MainStream"));

    if (!(access_token = proto::access_token::parse(CLOUD_TOKEN))) {
        return 1;
    }

    agent = agent::manager::create(std::move(cb), access_token, streams);
    if (agent == nullptr)
        bTerminate = true;

    if (!bTerminate && !agent->start())
        bTerminate = true;

    while (!bTerminate) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    agent->stop();
    logger->info("Agent stopped");
    agent = NULL;

    return 0;
}
