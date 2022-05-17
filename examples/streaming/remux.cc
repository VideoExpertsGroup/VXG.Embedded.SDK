#include <streamer/ffmpeg_remux_stream.h>
#include <streamer/ffmpeg_sink.h>
#include <streamer/ffmpeg_source.h>

#include <fstream>
#include <iostream>
#include <memory>

using namespace vxg::media;

int main(int argc, char**) {
    std::shared_ptr<ffmpeg::Source> src = std::make_shared<ffmpeg::Source>();
    std::shared_ptr<ffmpeg::Sink> sink = std::make_shared<ffmpeg::Sink>();

    // open the file:
    std::ifstream file("test.dav", std::ios::in | std::ios::binary);

    // read the data:
    auto b = std::make_shared<std::vector<uint8_t>>(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    auto o = std::make_shared<std::vector<uint8_t>>();

    inmem_remux_stream stream("dhav", b, "mov", o);
    stream.process();

    std::ofstream fout("test.mp4", std::ios::out | std::ios::binary);
    fout.write((const char*)(o->data()), o->size());
    fout.close();

    return 0;
}