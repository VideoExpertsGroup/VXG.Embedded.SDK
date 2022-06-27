#ifndef __FFMPEG_MULTIFILE_SINK_H
#define __FFMPEG_MULTIFILE_SINK_H

#include <functional>
#include "ffmpeg_sink.h"
#include "multifrag_sink.h"

namespace vxg {
namespace media {
namespace ffmpeg {
class multifile_sink : public MultiFrag::Sink {
    vxg::logger::logger_ptr logger {vxg::logger::instance(name())};

public:
    class formatted_ffmpeg_sink : public ffmpeg::Sink {
    public:
        std::string format_ {"mp4"};
        formatted_ffmpeg_sink(std::string format) : format_ {format} {}
        virtual bool init(std::string uri) override {
            return ffmpeg::Sink::init(uri, format_);
        }

        virtual std::string name() override { return format_ + "-ffmpeg-sink"; }
    };
    multifile_sink(MultiFrag::Sink::Mode mode = DURATION,
                   size_t param = 30,
                   std::string format = "mp4")
        : MultiFrag::Sink(std::make_shared<formatted_ffmpeg_sink>(format),
                          mode,
                          param),
          format_ {format} {}
    virtual ~multifile_sink() {}
    virtual std::string name() override { return "ffmpeg-multifile-sink"; }

    virtual bool init(std::string storage_path) {
        storage_path_ = storage_path;
        return MultiFrag::Sink::init();
    }

    std::string storage_path() { return storage_path_; }

protected:
    virtual std::string fragment_generate_uri(size_t id) override {
        std::string uri = storage_path_ + std::to_string(id) + "." + format_;

        logger->debug("New fragment id: {} generated uri: {}.", id, uri);

        return uri;
    }

    virtual void fragment_finalized(
        std::string uri,
        cloud::time begin,
        std::chrono::milliseconds duration) override {
        logger->debug("Fragment '{}' finalized.", uri);
    }

    std::string format_;
    std::string storage_path_ {"/tmp/"};
};  // class Sink
}  // namespace ffmpeg
}  // namespace media
}  // namespace vxg

#endif