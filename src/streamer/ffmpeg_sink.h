#ifndef __FFMPEG_SINK_H
#define __FFMPEG_SINK_H

#include "base_streamer.h"
#include "ffmpeg_common.h"

namespace vxg {
namespace media {
namespace ffmpeg {

//! @brief Base ffmpeg sink class
class Sink : public Streamer::ISink, public ffmpeg::common {
    vxg::logger::logger_ptr logger {vxg::logger::instance(name())};

public:
    Sink();
    virtual ~Sink();

    //! @brief Sink init
    //!
    //! @param url Output url
    //! @param fmt Output format
    //! @param data_buffer Output buffer for output to memory, if specified and
    //!        not nullptr the @p url will be ignored.
    //! @return true On success
    //! @return false On failure
    bool init(std::string url,
              std::string fmt,
              std::shared_ptr<std::vector<uint8_t>> data_buffer = nullptr);
    virtual bool init(std::string url = "");
    virtual bool finit();
    virtual void stop();
    virtual void error(Streamer::StreamError stream_error) {
        switch (stream_error) {
            case Streamer::StreamError::E_EOS: {
                // In case of EOS we should still handle all queued frames
                logger->info(
                    "Source reported EOS, handling rest queued frames");
                set_eos(true);
            } break;
            case Streamer::StreamError::E_FATAL: {
                logger->info("Source reported FATAL ERROR");
                set_eos(true);
                // Unblock ffmpeg calls on error
                set_ff_interrupted(true);
            } break;
            default:
                logger->warn("Unknown error {} from source, ignorring",
                             stream_error);
        }
    }

    virtual std::string name() { return "ffmpeg-sink"; }

    virtual bool droppable() { return false; }
    virtual bool negotiate(std::vector<Streamer::StreamInfo>);

private:
    virtual bool process(std::shared_ptr<Streamer::MediaFrame> frame);
    bool add_stream(Streamer::StreamInfo& info);

    bool video_key_frame_received_ {false};
};  // class Sink
}  // namespace ffmpeg
}  // namespace media
}  // namespace vxg
#endif