#include "base_streamer.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>

namespace vxg {
namespace media {

namespace Fake {
class Sink : public Streamer::ISink {
public:
    Sink() {}

    bool init() { return true; }
    bool finit() { return true; }

    std::string name() { return "FakeSink"; }

    virtual bool droppable() { return true; }
    virtual bool negotiate(std::vector<Streamer::StreamInfo> nego) {
        return true;
    }

private:
    bool process(std::shared_ptr<Streamer::MediaFrame> frame) {
        // spdlog::info("Process frame %p", frame.get());
        if (frame.get()) {
            spdlog::info("Frame type:{} %p key: {} len: %ld", frame->type,
                         frame->data.data(), frame->is_key, frame->len);
        }
        return true;
    }
};
}  // namespace Fake
}  // namespace media
}  // namespace vxg