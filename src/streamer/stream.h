#ifndef __STREAM_H
#define __STREAM_H

#include <map>
#include <memory>
#include <regex>

#include <streamer/base_streamer.h>
#include <utils/queued-handler.h>
#include <utils/utils.h>

namespace vxg {
namespace media {

//! @brief Media stream error namespace
namespace stream_error {
//! @brief Origin of the error
enum class origin : int {
    //! Source element of the media stream pipeline (ex. rtsp_source or custom
    //! source)
    O_SOURCE,
    //! Sink element of the media stream pipeline (ex. rtmp_sink or custom
    //! sink)
    O_SINK,
    //! Unknown origin
    O_UNKNOWN
};

//! @brief Error code
enum class error : int {
    //! Connection failed
    E_CONNECT,
    //! Processing failed, may be caused by peer connection problems
    E_PROCESS,
    //! Unknown error
    E_UNKNOWN
};

//! @brief Media stream error report simple object
struct report {
    //! Origin of the error
    origin from;
    //! Error code
    error code;
};
}  // namespace stream_error
//!
//! @brief base media stream abstract class
//! @details Media stream is the class representing media stream retranslation
//!          from the media source derived from the Streamer::ISource to the
//!          media sink derived from the Streamer::ISink.
//!          For instance, media stream could be a pair of `RTSP` source and
//!          `RTMP` sink, i.e. such media stream will be a retranslator of the
//!          `RTSP` stream to the `RTMP`
//!
class stream : public cloud::utils::queued_async_handler<stream_error::report> {
    vxg::logger::logger_ptr logger {vxg::logger::instance("base-media-stream")};

    void on_error(stream_error::origin origin, stream_error::error e) {
        logger->error("Error {} from {} received", e, origin);
        if (origin == stream_error::origin::O_SINK)
            stop_sink();
        if (origin == stream_error::origin::O_SOURCE) {
            stop_sink();
            stop();
        }

        if (on_error_cb_)
            on_error_cb_({origin, e});
    }

public:
    using on_error_cb = std::function<void(const stream_error::report& error)>;

public:
    //! @brief std::shared_ptr to the base_stream
    typedef std::shared_ptr<stream> ptr;

    //! @brief Construct a new base stream object
    //!
    //! @param name Unique stream name which will be used by the VXG Cloud API
    //! @param source Source object pointer
    //! @param sink Sink object pointer
    stream(std::string name,
           Streamer::ISource::ptr source,
           Streamer::ISink::ptr sink)
        : cloud::utils::queued_async_handler<stream_error::report>(),
          name_ {name},
          source_started_ {false},
          sink_started_ {false},
          cloud_name_ {
              cloud::utils::string_trim(name, std::regex("\\.|\\n|\\r"))},
          source_ {source},
          sink_ {sink} {
        source_->set_error_cb([&](Streamer::StreamError e) {
            on_error(stream_error::origin::O_SOURCE,
                     stream_error::error::E_PROCESS);
        });
        sink_->set_error_cb([&](Streamer::StreamError e) {
            on_error(stream_error::origin::O_SINK,
                     stream_error::error::E_PROCESS);
        });
    }
    virtual ~stream() {}

    //! @brief Initialize the source.
    //! @details Called by the internal code, derived class should allocate
    //!          and set base_stream::source_ with Streamer::ISink derived
    //!          object pointer.
    //!
    //! @param url source url
    //!
    //! @return true if successfully initialized source
    //! @return false if source initialization failed
    virtual bool init_source(std::string url) {
        std::lock_guard<std::recursive_mutex> guard(lock_);
        queued_async_handler::start();
        if (!source_->init(url)) {
            logger->error("Source unable to connect to stream");
            on_error(stream_error::origin::O_SOURCE,
                     stream_error::error::E_CONNECT);
            return false;
        }
        return true;
    }

    //! @brief Deinitialize source
    virtual void finit_source() {
        std::lock_guard<std::recursive_mutex> guard(lock_);
        queued_async_handler::stop();
        if (source_)
            source_->finit();
    }

    //! @brief Init media sink
    //! @details Derived class should allocate and initialize base_stream::sink_
    //!          with `RTMP` sink publishing media stream to the `RTMP` server
    //!          pointed by the \p uri
    //! @param[in] uri sink stream url if needed
    //! @return true Sink started
    //! @return false Sink start failed
    //!
    virtual bool init_sink(std::string uri) {
        std::lock_guard<std::recursive_mutex> guard(lock_);
        if (!sink_->init(uri)) {
            logger->error("Unable to init sink");
            on_error(stream_error::origin::O_SINK,
                     stream_error::error::E_CONNECT);
            return false;
        }
        return true;
    }

    //!
    //! @brief Deinitialize sink
    //! @details Derived class deinitialize and deallocates base_stream::sink_
    //!
    virtual void finit_sink() {
        std::lock_guard<std::recursive_mutex> guard(lock_);
        sink_->finit();
    }

    //! @private
    virtual bool start(std::string uri = "") {
        if (source_started_)
            return true;

        if (!source_) {
            logger->error("Source is null");
            goto fail;
        }

        if (!init_source(uri))
            goto fail;

        if (source_ && !source_->start()) {
            goto fail;
        }

        source_started_ = true;

        logger->info("Stream {} : source started", name());
        return true;
    fail:
        logger->error("Stream {} : source failed to start", name());
        finit_source();
        return false;
    }

    //! @private
    virtual void stop() {
        if (source_) {
            source_->stop();
            finit_source();
        }

        source_started_ = false;

        logger->info("Stream {}: source stopped", name());
    }

    //! @private
    //! @brief Start sink
    //!
    //! @param[in] url uri for sink if needed
    //! @return true Sink started
    //! @return false Failed to start sink
    //!
    virtual bool start_sink(std::string url = "") {
        if (sink_started_) {
            logger->warn("Publisher started, do nothing");
            return true;
        }

        if (!sink_) {
            logger->error("Publishing sink is null");
            goto fail;
        }

        if (!init_sink(url))
            goto fail;

        if (source_->linkSink(sink_))
            sink_started_ = sink_->start();

        if (!sink_started_) {
            finit_sink();
            goto fail;
        }

        logger->info("Stream {} : publishing sink started", name());
        return true;
    fail:
        logger->error("Stream {} : source failed to start", name());
        return false;
    }

    //! @private
    //! @brief Stop sink
    //!
    virtual void stop_sink() {
        if (sink_) {
            sink_->stop();
            if (source_)
                source_->unlinkSink(sink_);
            finit_sink();
        }
        logger->info("Stream {}: publisher stopped", name());

        sink_started_ = false;
    }

    //! @private
    virtual std::string name() { return name_; }
    //! @private
    virtual std::string cloud_name() { return cloud_name_; }
    //! @private
    virtual std::string video_es_name() { return cloud_name_ + "Video"; }
    //! @private
    virtual std::string audio_es_name() { return cloud_name_ + "Audio"; }
    //! @brief Set the on_error callback
    //!
    //! @param e_cb Callback
    virtual void set_on_error_cb(on_error_cb e_cb) {
        on_error_cb_ = e_cb;
        set_handler(e_cb);
    }

protected:
    //! @brief media source
    Streamer::ISource::ptr source_ {nullptr};
    //! @brief media sink
    Streamer::ISink::ptr sink_ {nullptr};
    //! @private
    std::string name_;
    //! @private
    std::string cloud_name_;
    //! @private
    bool source_started_;
    //! @private
    bool sink_started_;
    //! @private
    std::recursive_mutex lock_;
    //! @private
    on_error_cb on_error_cb_ {[](const stream_error::report& error) -> void {}};
};
}  // namespace media
}  // namespace vxg
#endif
