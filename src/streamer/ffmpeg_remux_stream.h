#pragma once

#include <streamer/base_streamer.h>
#include <streamer/ffmpeg_sink.h>
#include <streamer/ffmpeg_source.h>
#include <streamer/stream.h>

namespace vxg {
namespace media {
class inmem_remux_stream : public vxg::media::stream {
    const std::chrono::seconds MAX_REMUX_TIME_GUARD {30};

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
        std::promise<vxg::cloud::duration> barrier;
        std::future<vxg::cloud::duration> barrier_future = barrier.get_future();

        ff_sink->set_eos_cb([&barrier](vxg::cloud::duration d) {
            vxg::logger::info("Remux EOS");
            barrier.set_value(d);
        });

        std::future_status result = std::future_status::timeout;
        if (start() && start_sink())
            result = barrier_future.wait_for(MAX_REMUX_TIME_GUARD);
        stop_sink();
        stop();

        return (result == std::future_status::ready);
    }

private:
    std::shared_ptr<std::vector<uint8_t>> in_buf_;
    std::string in_fmt_;
    std::shared_ptr<std::vector<uint8_t>> out_buf_;
    std::string out_fmt_;
};

class source_mem_remux_stream : public vxg::media::stream {
    const std::chrono::seconds MAX_REMUX_TIME_GUARD {30};

public:
    source_mem_remux_stream(std::shared_ptr<Streamer::ISource> source)
        : vxg::media::stream("inmem-remuxer",
                             source,
                             std::make_shared<ffmpeg::Sink>()) {}

    virtual bool init_sink(std::string not_used = "") {
        auto sink = std::dynamic_pointer_cast<ffmpeg::Sink>(sink_);
        return sink->init("", out_fmt_, out_buf_);
    }

    bool process(std::string url,
                 std::string out_fmt,
                 std::shared_ptr<std::vector<uint8_t>> out_buf,
                 vxg::cloud::time::duration& duration) {
        auto ff_sink = std::dynamic_pointer_cast<ffmpeg::Sink>(sink_);
        std::promise<vxg::cloud::time::duration> barrier;
        std::future<vxg::cloud::time::duration> barrier_future =
            barrier.get_future();

        ff_sink->set_eos_cb([&barrier](vxg::cloud::time::duration d) {
            vxg::logger::info("Remux EOS");
            barrier.set_value(d);
        });

        out_fmt_ = out_fmt;
        out_buf_ = out_buf;

        if (start(url) && start_sink()) {
            auto result = barrier_future.wait_for(MAX_REMUX_TIME_GUARD);

            stop_sink();
            stop();

            if (result == std::future_status::ready)
                duration = barrier_future.get();

            return (result == std::future_status::ready);
        }

        stop_sink();
        stop();
        out_buf->clear();

        return false;
    }

private:
    std::shared_ptr<std::vector<uint8_t>> out_buf_;
    std::string out_fmt_;
};
}  // namespace media
}  // namespace vxg