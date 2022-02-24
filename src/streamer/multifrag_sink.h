#ifndef __MULTIPART_SINK_H
#define __MULTIPART_SINK_H

#include <functional>
#include "base_streamer.h"

#define DEFAULT_MP_FRAG_SIZE (1024 * 1024)
#define DEFAULT_MP_FRAG_DURATION (60)

namespace vxg {
namespace media {

namespace MultiFrag {
class Sink : public Streamer::ISink {
    vxg::logger::logger_ptr logger {vxg::logger::instance(name())};

public:
    //! Reason of new fragment start mode
    enum Mode {
        //! Fragment finalized after size exceeded
        SIZE = 1,
        //! Fragment finalized after duration exceeded
        DURATION = 2,

        UNKNOWN = -1
    };

    Sink(Streamer::ISink::ptr sink, MultiFrag::Sink::Mode mode, size_t param)
        : internal_sink_ {sink},
          mode_ {mode},
          mode_param_ {param},
          queue_ {std::shared_ptr<Streamer::MediaFrameQueue> {
              new Streamer::MediaFrameQueue(internal_sink_->droppable())}} {}

    virtual ~Sink() {}

    virtual bool init(std::string url = "");
    virtual bool finit();
    virtual std::string name() override { return "multifrag-sink"; }
    virtual bool negotiate(std::vector<Streamer::StreamInfo>);
    virtual bool droppable() { return false; }
    virtual void error(Streamer::StreamError e) {
        if (internal_sink_)
            return internal_sink_->error(e);
    }

protected:
    virtual std::string fragment_generate_uri(size_t fragment_id) = 0;
    virtual void fragment_finalized(
        std::string uri,
        std::chrono::time_point<std::chrono::system_clock> begin,
        std::chrono::milliseconds duration) = 0;
    std::chrono::milliseconds fragment_duration() { return fragment_duration_; }

private:
    virtual bool process(std::shared_ptr<Streamer::MediaFrame> frame);
    bool start_internal_sink();
    bool stop_internal_sink();

    bool init_internal_sink(size_t counter);
    bool finit_internal_sink(size_t counter);

    bool frag_need_finalize(std::shared_ptr<Streamer::MediaFrame> frame);
    void frag_update_stats(std::shared_ptr<Streamer::MediaFrame> frame);
    void frag_rotate_stats();

    Streamer::ISink::ptr internal_sink_;
    std::string templ_;
    Mode mode_;
    size_t mode_param_;
    std::vector<Streamer::StreamInfo> stream_info_;
    size_t size_;
    std::chrono::time_point<std::chrono::system_clock> fragment_start_ {
        std::chrono::system_clock::from_time_t(0)};
    std::chrono::milliseconds fragment_duration_;
    int64_t fragment_first_video_pts_ {0};
    int64_t fragment_first_video_dts_ {0};
    int64_t fragment_last_video_pts_ {0};
    int64_t fragment_last_video_dts_ {0};
    int64_t fragment_first_audio_pts_ {0};
    int64_t fragment_first_audio_dts_ {0};
    int64_t fragment_last_audio_pts_ {0};
    int64_t fragment_last_audio_dts_ {0};
    std::shared_ptr<Streamer::MediaFrameQueue> queue_;
    size_t counter_ {0};
    std::string frag_uri_;
    bool video_key_frame_received_ {false};
    bool disable_audio_ {false};
};  // class Sink
}  // namespace MultiFrag
}  // namespace media
}  // namespace vxg

#endif