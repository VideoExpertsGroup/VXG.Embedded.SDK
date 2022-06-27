#include <streamer/recording_stream.h>

using namespace vxg::media;

int main(int argc, char** argv) {
    rtsp_segmented_mp4_recording_stream stream;
    std::string srcUrl;

    if (argc == 2)
        srcUrl = argv[1];
    else
        srcUrl = "rtsp://DEFAULT_URL";

    stream.start(srcUrl, "/mnt/e/record-test/");

    while (stream.is_running())
        std::this_thread::sleep_for(std::chrono::seconds(1));

    stream.stop();

    return 0;
}