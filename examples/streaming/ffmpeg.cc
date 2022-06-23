#include <streamer/ffmpeg_sink.h>
#include <streamer/ffmpeg_source.h>

using namespace vxg::media;

int main(int argc, char** argv) {
    auto sink = std::make_shared<ffmpeg::Sink>();
    ffmpeg::Source src;
    std::string srcUrl;
    AVDictionary* opts = NULL;
    av_dict_set(&opts, "rtsp_transport", "tcp", AV_OPT_SEARCH_CHILDREN);

    if (argc == 2)
        srcUrl = argv[1];
    else
        srcUrl = "rtsp://DEFAULT_URL";

    if (!sink->init("rtmp://54.173.34.172:1937/publish_demo/test_ffstream")) {
        spdlog::error("Unable to connect to stream");
        goto stop;
    }

    if (!src.init(srcUrl, opts)) {
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