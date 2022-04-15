#include <signal.h>

#include <args.hxx>

//! [Includes and namespaces]
#include <agent/manager.h>
#include <agent/rtsp-stream.h>
#include <utils/logging.h>

using namespace vxg::cloud;
using namespace vxg::cloud::agent;
//! [Includes and namespaces]

static bool quit = 0;

#if !defined(_WIN32)
static void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        fprintf(stderr, "\nSIGTERM received\n\n");
        quit = true;
    }
}
#endif

//! [Common callback class implementation]
using namespace vxg::cloud;
//! @private
class my_agent_callback : public agent::callback {
public:
    //! [Bye command callback]
    virtual void on_bye(proto::bye_reason reason) override {
        vxg::logger::error("Error {}", json(reason).dump());
    }
    //! [Bye command callback]

    //! [Raw message callback]
    virtual bool on_raw_msg(std::string client_id, std::string& data) override {
        vxg::logger::info("Raw message {} from client '{}'", data, client_id);

        // Reply json
        data = "{\"reply\": \"OK\"}";

        return true;
    }
    //! [Get message callback]

    //! [Get logs callback]
    virtual bool on_get_log(std::string& log_data) override {
        log_data = "log messages...";
        vxg::logger::warn("{} not implemented", __func__);
        return true;
    }
    //! [Get logs callback]

    //! [Backward audio start callback]
    virtual bool on_start_backward_audio(std::string url) override {
        // Start backward audio playback from url
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }
    //! [Backward audio start callback]

    //! [Backward audio stop callback]
    virtual bool on_stop_backward_audio(std::string url) override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }
    //! [Backward audio stop callback]

    //! [Get video input config callback]
    virtual bool on_get_cam_video_config(proto::video_config& config) override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }
    //! [Get video input config callback]

    //! [Set video input config]
    virtual bool on_set_cam_video_config(
        const proto::video_config& config) override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }
    //! [Set video input config]

    //! [Get audio input configuration]
    virtual bool on_get_cam_audio_config(proto::audio_config& config) override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }
    //! [Get audio input configuration]

    //! [Set audio input/output config]
    virtual bool on_set_cam_audio_config(
        const proto::audio_config& config) override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }
    //! [Set audio input/output config]

    //! [Get PTZ config]
    virtual bool on_get_ptz_config(proto::ptz_config& config) override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }
    //! [Get PTZ config]

    //! [PTZ command]
    virtual bool on_cam_ptz(proto::ptz_command& command) override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }
    //! [PTZ command]

    //! [Get OSD config]
    virtual bool on_get_osd_config(proto::osd_config& config) override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }
    //! [Get OSD config]

    //! [Set OSD config]
    virtual bool on_set_osd_config(const proto::osd_config& config) override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }
    //! [Set OSD config]

    //! [Get WiFi config]
    virtual bool on_get_wifi_config(proto::wifi_config& config) override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }
    //! [Get WiFi config]

    //! [Set WiFi config]
    virtual bool on_set_wifi_config(
        const proto::wifi_network& config) override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }
    //! [Set WiFi config]

    //! [Get motion detection configuration]
    virtual bool on_get_motion_detection_config(
        proto::motion_detection_config& config) override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }
    //! [Get motion detection configuration]

    //! [Set motion detection config]
    virtual bool on_set_motion_detection_config(
        const proto::motion_detection_config& config) override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }
    //! [Set motion detection config]

    //! [Get events configuration]
    virtual bool on_get_cam_events_config(
        proto::events_config& config) override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }
    //! [Get events configuration]

    //! [Set motion detection config]
    virtual bool on_set_cam_events_config(
        const proto::events_config& config) override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }
    //! [Set motion detection config]

    //! [Get device timezone in IANA format]
    virtual bool on_get_timezone(std::string& timezone) override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }
    //! [Get device timezone in IANA format]

    //! [Set device timezone in IANA format]
    virtual bool on_set_timezone(std::string timezone) override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }
    //! [Set device timezone in IANA format]

    //! [Get memory card information]
    virtual bool on_get_memorycard_info(
        proto::event_object::memorycard_info_object& info) override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }
    //! [Get memory card information]
};
//! [Common callback class implementation]

//! [Media stream callback class implementation]
//! @private
class my_media_stream : public media::rtsp_stream {
public:
    my_media_stream(std::string url, std::string name)
        : media::rtsp_stream(url, name) {}

    bool get_supported_stream(proto::supported_stream_config& config) override {
        vxg::logger::warn("{} default implementation should be overriden",
                          __func__);

        config.id = cloud_name();
        config.video = "Video" + std::to_string(0);
        // config.audio = "Audio" + std::to_string(0);

        return true;
    }

    virtual bool get_stream_caps(proto::stream_caps& caps) override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    virtual bool get_stream_config(
        proto::stream_config& streamConfig) override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    virtual bool set_stream_config(
        const proto::stream_config& streamConfig) override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    virtual bool get_snapshot(
        proto::event_object::snapshot_info_object& snapshot) override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    virtual std::vector<proto::video_clip_info> record_get_list(
        vxg::cloud::time begin,
        vxg::cloud::time end,
        bool align) override {
        std::vector<proto::video_clip_info> empty_vector(0);
        vxg::logger::warn("{} not implemented", __func__);
        return empty_vector;
    }

    virtual proto::video_clip_info record_export(
        vxg::cloud::time begin,
        vxg::cloud::time end) override {
        proto::video_clip_info clip;
        vxg::logger::warn("{} not implemented", __func__);

        // empty clip
        return clip;
    }

    virtual bool start_record() override {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    virtual bool stop_record() override {
        vxg::logger::warn("{} not implemented", __func__);
        return true;
    }
};
//! [Media stream callback class implementation]

//! [Event stream callback class implementation]
//! @private
class my_event_stream : public agent::event_stream {
public:
    my_event_stream(std::string name) : agent::event_stream(name) {}

    //! [Start events generation], called by internal code when the events
    virtual bool start() {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }

    //! [Stop events generation]
    virtual void stop() { vxg::logger::warn("{} not implemented", __func__); }

    //! [Initialize the events generation]
    virtual bool init() {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }
    //! [Initialize the events generation]

    //! [Deinitialize the events generation]
    virtual void finit() { vxg::logger::warn("{} not implemented", __func__); }
    //! [Deinitialize the events generation]

    //! [Turn on/off the event_stream triggered recording]
    virtual bool set_trigger_recording(bool enabled, int pre, int post) {
        vxg::logger::warn("{} not implemented", __func__);
        return false;
    }
    //! [Turn on/off the event_stream triggered recording]

    virtual bool get_events(std::vector<proto::event_config>& configs) {
        return false;
    }

    virtual bool set_events(const std::vector<proto::event_config>& config) {
        return false;
    }
};
//! [Event stream callback class implementation]

std::string vxg_cloud_token;
std::string rtsp_url;

bool parse_args(int argc, char** argv) {
    args::ArgumentParser parser("This is a test program.", "");
    args::HelpFlag help(parser, "help", "Display this help menu",
                        {'h', "help"});
    args::CompletionFlag completion(parser, {"complete"});
    args::Positional<std::string> token(parser, "vxg_cloud_token",
                                        "VXG Cloud Access Token", "",
                                        args::Options::Required);
    args::Positional<std::string> url(parser, "rtsp_url", "RTSP stream url", "",
                                      args::Options::Required);

    try {
        parser.ParseCLI(argc, argv);
        vxg_cloud_token = args::get(token);
        rtsp_url = args::get(url);
    } catch (const args::RequiredError& e) {
        std::cout << e.what() << std::endl;
        return false;
    } catch (const args::Completion& e) {
        std::cout << e.what();
        return false;
    } catch (const args::Help&) {
        std::cout << parser;
        return false;
    } catch (const args::ParseError& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return false;
    }
    return true;
}

int main(int argc, char** argv) {
    // Parse args and retreive token and rtsp url
    if (!parse_args(argc, argv))
        return EXIT_FAILURE;

#if !defined(_WIN32)
    // Catch signal
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, signal_handler);
#endif

    vxg::logger::info("VXG Cloud Agent Library Version: {}",
                      vxg::cloud::agent::version());

    //! [Create and start agent object]
    using namespace vxg::cloud::agent;
    // Agent
    manager::ptr agent;

    // VXG Cloud token
    proto::access_token::ptr access_token =
        proto::access_token::parse(vxg_cloud_token);

    // Agent callback
    callback::ptr cb = std::make_unique<my_agent_callback>();

    // Media stream
    std::vector<agent::media::stream::ptr> streams;
    media::stream::ptr stream =
        std::make_shared<my_media_stream>(rtsp_url, "MyMediaStream");
    streams.push_back(stream);

    // Event stream
    std::vector<agent::event_stream::ptr> event_streams;
    event_stream::ptr event_stream =
        std::make_shared<my_event_stream>("MyEventStream");
    event_streams.push_back(event_stream);

    // Create agent
    if ((agent = agent::manager::create(std::move(cb), access_token, streams,
                                        event_streams)) == nullptr) {
        vxg::logger::error("Failed to create agent");
        return EXIT_FAILURE;
    }

    if (!quit && !agent->start())
        quit = true;
    //! [Create and start agent object]

    // Spin main thread until stopped
    while (!quit) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    //! [Stop agent]
    agent->stop();
    agent = NULL;
    //! [Stop agent]

    vxg::logger::info("Agent stopped");

    return EXIT_SUCCESS;
}
