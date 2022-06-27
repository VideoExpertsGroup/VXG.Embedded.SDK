#include <signal.h>

#include <args.hxx>

//! [Includes and namespaces]
#include <agent/manager.h>
#include <agent/rtsp-stream.h>
#include <utils/logging.h>
#include <utils/properties.h>

using namespace vxg::cloud;
using namespace vxg::cloud::agent;
//! [Includes and namespaces]

agent::config agent_config;

static bool quit = 0;
static vxg::properties props;
#if !defined(_WIN32)
static void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        fprintf(stderr, "\nSIGTERM received\n\n");
        quit = true;
    }
}
#endif

//! [Minimal callback class implementation]
using namespace vxg::cloud;
//! @private
class agent_callback_minimal : public agent::callback {
public:
    virtual void on_bye(proto::bye_reason reason) override {
        vxg::logger::warn("Connection close {}", json(reason).dump());
    }

    virtual void on_registered(const std::string& sid) override {
        // Save Cloud registration session id in the local properties file.
        // This is required for the fast reconnection to the Cloud.
        props.set("prev_sid", sid);
    }
};
//! [Minimal callback class implementation]

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
    args::Flag secure_connection_arg(
        parser, "",
        "Use secure cloud connetion(enables encryption, cloud agent library "
        "must be compiled with openssl support enabled)",
        {"secure-channel", 's'});

    try {
        parser.ParseCLI(argc, argv);
        vxg_cloud_token = args::get(token);
        rtsp_url = args::get(url);
        agent_config.insecure_cloud_channel =
            !args::get(secure_connection_arg);
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
    vxg::properties::reset("agent-test.props");

    // Try to load and set previously saved session id.
    // This is required for the fast reconnection to the Cloud.
    if (!props.get("prev_sid").empty())
        agent_config.cm_registration_sid = props.get("prev_sid");

    // Parse args and retrieve token and rtsp url
    if (!parse_args(argc, argv))
        return EXIT_FAILURE;

#if !defined(_WIN32)
    // Catch signal
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);
#endif

    vxg::logger::info("VXG Cloud Agent Library Version: {}",
                      vxg::cloud::agent::version());

    //! [Create and start agent object]
    using namespace vxg::cloud::agent;
    // Agent
    manager::ptr agent;

    // VXG Cloud token
    auto access_token =
        proto::access_token::parse(vxg_cloud_token);

    // Agent callback
    callback::ptr cb = std::make_unique<agent_callback_minimal>();

    // Media stream
    std::vector<agent::media::stream::ptr> streams;
    media::stream::ptr stream =
        std::make_shared<media::rtsp_stream>(rtsp_url, "DemoStream");
    streams.push_back(stream);

    // Create agent
    if ((agent = agent::manager::create(agent_config, std::move(cb),
                                        access_token, streams)) == nullptr) {
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
    agent = nullptr;
    //! [Stop agent]

    vxg::logger::info("Agent stopped");

    return EXIT_SUCCESS;
}
