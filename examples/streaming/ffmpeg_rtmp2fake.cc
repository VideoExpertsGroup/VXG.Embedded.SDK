#include <streamer/fake_sink.h>
#include <streamer/ffmpeg_multifile_sink.h>
#include <streamer/ffmpeg_source.h>

#include <net/http.h>

#include <args.hxx>

using namespace vxg::cloud;
using namespace vxg::cloud::transport::libwebsockets;
using namespace vxg::media;
using namespace vxg::media::Streamer;

std::string g_source_url = "";
std::string g_sink_url = "";

bool parse_args(int argc, char** argv) {
    args::ArgumentParser parser("This is a test program.", "");
    args::HelpFlag help(parser, "help", "Display this help menu",
                        {'h', "help"});
    args::CompletionFlag completion(parser, {"complete"});
    args::Positional<std::string> src_url(
        parser, "source_url", "Source stream url", "", args::Options::Required);
    args::Positional<std::string> sink_url(
        parser, "sink_url", "Sink stream url", "", args::Options::Required);

    try {
        parser.ParseCLI(argc, argv);
        g_source_url = args::get(src_url);
        g_sink_url = args::get(sink_url);
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

    if (g_source_url.empty() || g_sink_url.empty())
        return false;

    return true;
}

class http_sink : public ffmpeg::Sink {
    std::string url_;

public:
    virtual std::string name() { return "ffmpeg_http_sink"; }

    // -channels 1 -ab 32000 -ar 8000 -ac 1 -c:a adpcm_g726le -content_type
    // audio/G726-32 -multiple_requests 1 -f g726le -channels 1 -ab 128k -ar
    // 8000 -ac 1 -c:a pcm_mulaw -content_type audio/basic -chunked_post 0
    // -multiple_requests 1
    virtual bool negotiate(std::vector<Streamer::StreamInfo> info) {
        AVDictionary* opts = nullptr;
        std::string content_type = "";

        for (auto& i : info) {
            if (i.type == StreamInfo::ST_AUDIO) {
                switch (i.audio.codec) {
                    case StreamInfo::AC_G711_U:
                        content_type = "audio/basic";
                        break;
                    case StreamInfo::AC_AAC:
                        content_type = "audio/aac";
                        break;
                    case StreamInfo::AC_OPUS:
                        content_type = "audio/opus";
                        break;
                    case StreamInfo::AC_LPCM:
                        content_type = "audio/lpcm";
                        break;
                    case StreamInfo::AC_G726:
                        content_type = "audio/G726-24";
                        break;
                    default:
                        vxg::logger::warn("No mime type for audio codec {}",
                                          i.audio.codec);
                        break;
                }
            }
        }

        if (!content_type.empty())
            av_dict_set(&opts, "content_type", content_type.c_str(),
                        AV_OPT_SEARCH_CHILDREN);
        av_dict_set(&opts, "multiple_requests", "1", AV_OPT_SEARCH_CHILDREN);
        av_dict_set(&opts, "chunked_post", "0", AV_OPT_SEARCH_CHILDREN);
        av_dict_set(&opts, "reconnect", "1", AV_OPT_SEARCH_CHILDREN);

        set_ff_opts(opts);

        return ffmpeg::Sink::negotiate(info);
    }
};

int main(int argc, char** argv) {
    auto sink = std::make_shared<http_sink>();
    ffmpeg::Source src;
    AVDictionary* opts = NULL;

    av_dict_set(&opts, "stimeout", "5000000", AV_OPT_SEARCH_CHILDREN);

    if (!parse_args(argc, argv))
        return EXIT_FAILURE;

    if (!sink->init(g_sink_url, "s8")) {
        spdlog::error("Unable to init sink {}", sink->name());
        goto stop;
    }

    if (!src.init(g_source_url, opts)) {
        spdlog::error("Unable to connect to stream");
        goto stop;
    }

    src.linkSink(sink);

    src.start();
    sink->start();

    while (src.is_running())
        std::this_thread::sleep_for(std::chrono::seconds(1));

stop:
    src.stop();
    sink->stop();

    src.unlinkSink(sink);

    src.finit();
    sink->finit();

    return 0;
}