#pragma once

#include <agent-proto/proto.h>
#include <agent/stream.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace media {

enum recording_mode_flags {
    NONE = 0,
    LOCAL = 1L << 0,
    LOCAL_BY_EVENT = 1L << 1,
    ALL = ((LOCAL_BY_EVENT << 1) - 1)
};
inline _GLIBCXX_CONSTEXPR recording_mode_flags
operator&(recording_mode_flags __a, recording_mode_flags __b) {
    return recording_mode_flags(static_cast<int>(__a) & static_cast<int>(__b));
}

inline _GLIBCXX_CONSTEXPR recording_mode_flags
operator|(recording_mode_flags __a, recording_mode_flags __b) {
    return recording_mode_flags(static_cast<int>(__a) | static_cast<int>(__b));
}

inline _GLIBCXX_CONSTEXPR recording_mode_flags
operator^(recording_mode_flags __a, recording_mode_flags __b) {
    return recording_mode_flags(static_cast<int>(__a) ^ static_cast<int>(__b));
}

inline _GLIBCXX_CONSTEXPR recording_mode_flags
operator~(recording_mode_flags __a) {
    return recording_mode_flags(~static_cast<int>(__a));
}

inline const recording_mode_flags& operator|=(recording_mode_flags& __a,
                                              recording_mode_flags __b) {
    return __a = __a | __b;
}

inline const recording_mode_flags& operator&=(recording_mode_flags& __a,
                                              recording_mode_flags __b) {
    return __a = __a & __b;
}

inline const recording_mode_flags& operator^=(recording_mode_flags& __a,
                                              recording_mode_flags __b) {
    return __a = __a ^ __b;
}

class stream_manager {
    vxg::logger::logger_ptr logger = vxg::logger::instance("stream-manager");
    std::vector<stream_ptr> streams_;
    stream_ptr recording_stream_ {nullptr};
    stream_ptr live_stream_ {nullptr};
    stream_ptr by_event_stream_ {nullptr};

private:
    struct recording_status {
        recording_mode_flags mode_flags {recording_mode_flags::NONE};
        bool started {false};
        std::string stream_id;
        proto::stream_config config;
        cloud::time last_start_time;
        size_t external_stop_count;

        bool enabled() { return !!mode_flags; }
        bool need_start() { return (!!mode_flags) && !started; }
    };

    struct live_status {
        std::string stream_id;
        size_t sinks {0};
        int record_event_depth;
        std::string url;
    };

    struct stream_status {
        media::stream::ptr stream;
        recording_status recording;
        live_status live;
        bool source_started;
    };
    std::map<media::stream::ptr, stream_status> stream_status_;

public:
    stream_manager() {}
    ~stream_manager() {}

    void set_streams(std::vector<stream::ptr> streams) {
        streams_ = streams;

        if (streams_.empty()) {
            logger->warn(
                "No media streams were provided by user, working in an idle "
                "mode!");
        } else {
            live_stream_ = by_event_stream_ = recording_stream_ = streams_[0];
            logger->info(
                "Setting stream {} as default for live, by_event and recording "
                "for now",
                streams_[0]->cloud_name());
        }

        // for (auto& s : streams) {
        // std::weak_ptr<stream> w(s);
        // s->set_on_error_cb([this, w](vxg::media::Streamer::StreamError e)
        // {
        //     if (auto s = w.lock()) {
        //         logger->info("Trying to restart disconnected stream {}",
        //                      s->cloud_name());
        //         if (stream_status_[s].source_started)
        //             s->start();
        //         if (stream_status_[s].live.sinks)
        //             s->start_sink(stream_status_[s].live.url);
        //         if (stream_status_[s].recording.started &&
        //             s->record_needs_source())
        //             s->start_record();
        //     }
        // });
        // }
    }

    bool start_live(const std::string& stream_id, const std::string& url) {
        stream::ptr stream = lookup_stream(stream_id);
        bool result = false;

        logger->debug("Start LIVE; stream {}, sinks {}", stream_id,
                      stream_status_[stream].live.sinks);

        if (stream) {
            // Start media source if it wasn't started already
            if (!stream_status_[stream].source_started &&
                !(stream_status_[stream].source_started = stream->start())) {
                logger->error("Unable to start media source for live");
                result = false;
            }

            if (stream_status_[stream].source_started) {
                // Start only if no sink was already started, but we need to
                // count how many times sink start requests were issued
                if (stream_status_[stream].live.sinks == 0) {
                    result = stream->start_sink(url);
                    if (result)
                        stream_status_[stream].live.url = url;
                    live_stream_ = stream;
                } else
                    result = true;
                stream_status_[stream].live.sinks += result;
            }
        }

        return result;
    }

    bool stop_live() {
        logger->info("Stop LIVE; stream {}, sinks {}",
                     live_stream_->cloud_name(),
                     stream_status_[live_stream_].live.sinks);

        if (live_stream_) {
            // Real stop happens only when all requested sink starts will be
            // stopped
            if (stream_status_[live_stream_].live.sinks &&
                !--stream_status_[live_stream_].live.sinks) {
                live_stream_->stop_sink();
                logger->info("Live for stream {} was stopped",
                             live_stream_->cloud_name());
            }
        }

        // Returns true if we don't have sinks anymore
        return !(live_stream_ && stream_status_[live_stream_].live.sinks);
    }

    bool check_stop_media_source(stream_ptr stream) {
        // Don't stop source if:
        // - Any live sinks are presented
        // - Recording enabled and record_needs_source()
        // - Timeline sync mode is by_event via live(starting new source is
        // slow)
        if (stream_status_[stream].live.sinks == 0 &&
            (!stream_status_[stream].recording.enabled() ||
             !stream->record_needs_source()) &&
            stream_status_[stream].source_started) {
            logger->debug(
                "No media data consumers for media source, stopping it");
            stream_status_[stream].source_started = false;
            stream->stop();
            return true;
        }
        return false;
    }

    bool start_recording(std::string stream_name, recording_mode_flags mode) {
        media::stream::ptr stream = lookup_stream(stream_name);

        if (stream) {
            recording_stream_ = stream;
            logger->info(
                "Recording start request for stream {} mode: {:x}:{:x}, "
                "started: {}",
                stream_name, mode, stream_status_[stream].recording.mode_flags,
                stream_status_[stream].recording.started);

            if (!stream_status_[stream].recording.mode_flags &&
                !stream_status_[stream].recording.started) {
                stream_status_[stream].recording.started +=
                    stream->start_record();
                stream_status_[stream].recording.last_start_time =
                    utils::time::now();
                stream_status_[stream].recording.stream_id = stream_name;
                // TODO: we need to track recording stream config
            } else {
                logger->info("Recording start not required");
            }
            stream_status_[stream].recording.mode_flags |= mode;

            return stream_status_[stream].recording.started;
        } else {
            logger->error("No way to start recording for unknown stream {}",
                          stream_name);
        }

        return false;
    }

    bool stop_recording(recording_mode_flags mode, bool force = false) {
        if (!recording_stream_) {
            logger->debug("Unable to stop recording since it was not started");
            return false;
        }

        logger->info("Recording stop mode {:x} out of {:x}, started: {}", mode,
                     stream_status_[recording_stream_].recording.mode_flags,
                     stream_status_[recording_stream_].recording.started);

        if ((stream_status_[recording_stream_].recording.mode_flags & mode) &&
            stream_status_[recording_stream_].recording.started) {
            stream_status_[recording_stream_].recording.mode_flags &= ~mode;

            if (!stream_status_[recording_stream_].recording.mode_flags) {
                logger->info("Recording for all modes stopped");
                recording_stream_->stop_record();
                stream_status_[recording_stream_].recording.started = false;
                recording_stream_ = nullptr;
            } else {
                logger->info("Recording for modes {:x} stopped", mode);
            }
        } else {
            logger->info("Recording stop not required");
        }
        return !stream_status_[recording_stream_].recording.started;
    }

    stream_ptr lookup_stream(std::string name) {
        for (auto& s : streams_) {
            if (s->cloud_name() == name)
                return s;
        }

        return nullptr;
    }

    stream_status& status(stream_ptr s) { return stream_status_[s]; }
    stream_status& status(std::string stream_id) {
        return stream_status_[lookup_stream(stream_id)];
    }

    recording_status& recording_status() {
        return stream_status_[recording_stream_].recording;
    }

    live_status& live_status() { return stream_status_[live_stream_].live; }

    stream_ptr recording_stream() const { return recording_stream_; }

    bool start_media_source(std::string stream_id) {
        auto stream = lookup_stream(stream_id);

        if (!(stream_status_[stream].source_started = stream->start())) {
            logger->error("Unable to start media source for stream {}",
                          stream_id);
            return false;
        }
        return true;
    }

    void set_stream_for_by_event(std::string stream_id) {
        auto s = lookup_stream(stream_id);

        if (s) {
            by_event_stream_ = s;
        }
    }

    stream_ptr by_event_stream() { return by_event_stream_; }

    stream_ptr preview_stream() { return by_event_stream_; }

    stream_ptr snapshot_stream() { return by_event_stream_; }

    stream_ptr live_stream() { return live_stream_; }

    void stop_all_streams() {
        if (recording_stream_)
            stop_recording(recording_status().mode_flags);

        while (!stop_live())
            ;

        if (live_stream_)
            check_stop_media_source(live_stream_);
    }
};
using stream_manager_ptr = std::shared_ptr<stream_manager>;
}  // namespace media
}  // namespace agent
}  // namespace cloud
}  // namespace vxg