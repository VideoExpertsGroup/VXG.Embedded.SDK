#include <streamer/rtmp_sink.h>
#include <streamer/rtsp_source.h>

using namespace vxg::media;

int main(int argc, char** argv) {
    auto sink = std::make_shared<rtmp_sink>(nullptr);
    rtsp_source src(std::string("tcp"), {Streamer::VIDEO, Streamer::AUDIO});
    std::string srcUrl;

    if (argc == 2)
        srcUrl = argv[1];
    else
        srcUrl = "rtsp://admin:password@10.20.16.55/path";

    if (!sink->init("rtmp://54.173.34.172:1937/publish_demo/test_ffstream")) {
        spdlog::error("Unable to connect to stream");
        goto stop;
    }

    if (!src.init(srcUrl)) {
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