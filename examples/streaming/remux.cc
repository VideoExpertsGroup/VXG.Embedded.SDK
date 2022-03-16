#include <streamer/ffmpeg_sink.h>
#include <streamer/ffmpeg_source.h>

#include <streamer/stream.h>

#include <fstream>
#include <iostream>
#include <memory>

using namespace vxg::media;

class inmem_remux_stream : public vxg::media::stream {
public:
    inmem_remux_stream(std::string in_fmt,
                       std::shared_ptr<std::vector<uint8_t>> in_buf,
                       std::string out_fmt,
                       std::shared_ptr<std::vector<uint8_t>> out_buf)
        : vxg::media::stream("inmem-remuxer",
                             std::make_shared<ffmpeg::Source>(),
                             std::make_shared<ffmpeg::Sink>()),
          in_fmt_ {in_fmt},
          out_fmt_ {out_fmt},
          in_buf_ {in_buf},
          out_buf_ {out_buf} {}

    virtual bool init_source(std::string not_used = "") override {
        auto source = std::dynamic_pointer_cast<ffmpeg::Source>(source_);
        return source->init(in_buf_, 0, in_fmt_);
    }

    virtual bool init_sink(std::string not_used = "") {
        auto sink = std::dynamic_pointer_cast<ffmpeg::Sink>(sink_);
        return sink->init("", out_fmt_, out_buf_);
    }

    bool process() {
        auto ff_sink = std::dynamic_pointer_cast<ffmpeg::Sink>(sink_);
        std::promise<void> barrier;
        std::future<void> barrier_future = barrier.get_future();

        ff_sink->set_eos_cb([&barrier]() {
            vxg::logger::instance("remux")->info("EOS callback");
            vxg::logger::instance("remux")->flush();
            barrier.set_value();
        });

        start();
        start_sink();

        barrier_future.wait();

        stop_sink();
        stop();

        return true;
    }

private:
    std::shared_ptr<std::vector<uint8_t>> in_buf_;
    std::string in_fmt_;
    std::shared_ptr<std::vector<uint8_t>> out_buf_;
    std::string out_fmt_;
};

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