#pragma once
#ifndef __BASE_STREAMER_H
#define __BASE_STREAMER_H

#include <cstdlib>
#include <future>
#include <map>
#include <queue>
#include <string>

#if __cplusplus <= 199711L
#include <pthread.h>
#else
#include <mutex>
#include <thread>
#endif

#include <streamer/stats.h>
#include <utils/logging.h>
#include <utils/utils.h>

namespace vxg {
namespace media {
namespace Streamer {
constexpr int SINK_THREAD_PRIO = 110;
constexpr int SRC_THREAD_PRIO = 120;
enum DropDirection {
    DROP_FRONT,
    DROP_BACK,
};

//! @brief Stream error
enum StreamError {
    E_NONE,
    E_FATAL,
    E_EOS,
    // ...
};

//! Concurent Thread-Safe Queue
//! @private
template <typename T>
class Queue {
    vxg::logger::logger_ptr logger {vxg::logger::instance("media-queue")};

public:
    Queue(bool drop = false, DropDirection drop_direction = DROP_FRONT)
        : flushing_(false),
          drop_(drop),
          drop_direction_(drop_direction),
          wait_video_key_(false) {
#if __cplusplus <= 199711L
        pthread_mutex_init(&mutex_, NULL);
        pthread_cond_init(&cond_, NULL);
#endif
    }

    virtual ~Queue() {
#if __cplusplus <= 199711L
        pthread_mutex_destroy(&mutex_);
        pthread_cond_destroy(&cond_);
#endif
    }

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    T pop() {
        T item = nullptr;
#if __cplusplus <= 199711L
        pthread_mutex_lock(&mutex_);
        while (queue_.empty() && !flushing_ && !eos_)
            pthread_cond_wait(&cond_, &mutex_);
        if (!queue_.empty()) {
            item = std::move(queue_.front());
            queue_.pop_front();
        }
        pthread_cond_signal(&cond_);
        pthread_mutex_unlock(&mutex_);
#else
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock,
                   [=]() { return !queue_.empty() || flushing_ || eos_; });

        if (!queue_.empty()) {
            item = std::move(queue_.front());
            queue_.pop_front();
        }
        cond_.notify_one();
#endif
        return item;
    }

    const T* front() {
        const T* item = nullptr;
#if __cplusplus <= 199711L
        pthread_mutex_lock(&mutex_);
        if (!queue_.empty())
            item = &queue_.front();
        pthread_mutex_unlock(&mutex_);
#else
        std::unique_lock<std::mutex> lock(mutex_);
        if (!queue_.empty())
            item = &queue_.front();
#endif
        return item;
    }

    void pop(T& item) {
#if __cplusplus <= 199711L
        pthread_mutex_lock(&mutex_);
        while (queue_.empty() && !flushing_)
            pthread_cond_wait(&cond_, &mutex_);
        if (!queue_.empty() && !flushing_) {
            item = queue_.front();
            queue_.pop_front();
        } else {
            item = nullptr;
        }
        pthread_cond_signal(&cond_);
        pthread_mutex_unlock(&mutex_);
#else
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [=]() { return !queue_.empty() || flushing_; });
        if (!queue_.empty() && !flushing_) {
            item = queue_.front();
            queue_.pop_front();
        } else {
            item = nullptr;
        }
        cond_.notify_one();
#endif
    }

    bool full() { return queue_.size() >= max_items; }

    void push(const T& item) {
#if __cplusplus <= 199711L
        pthread_mutex_lock(&mutex_);
#else
        std::unique_lock<std::mutex> lock(mutex_);
#endif

        if (!flushing_) {
#if __cplusplus <= 199711L
            while (full && !drop_)
                pthread_cond_wait(&cond_, &mutex_);
#else
            cond_.wait(lock, [=]() { return !full() || drop_; });
#endif

            switch (drop_direction_) {
                case DROP_BACK: {
                    if (!_can_drop(full(), item)) {
                        queue_.push_back(item);
                    }
                    // else drop
                } break;
                case DROP_FRONT: {
                    while (!queue_.empty() &&
                           _can_drop(full(), queue_.front())) {
                        // drop old frames if we can
                        queue_.pop_front();
                    }
                    queue_.push_back(item);
                } break;
                default: {
                    logger->warn("Wrong drop direction");
                } break;
            }
        }
#if __cplusplus <= 199711L
        pthread_cond_signal(&cond_);
        pthread_mutex_unlock(&mutex_);
#else
        cond_.notify_one();
#endif
    }

    void push(T&& item) {
#if __cplusplus <= 199711L
        pthread_mutex_lock(&mutex_);
#else
        std::unique_lock<std::mutex> lock(mutex_);
#endif

        if (!flushing_) {
            bool full = queue_.size() >= max_items;

#if __cplusplus <= 199711L
            while (full && !drop_)
                pthread_cond_wait(&cond_, &mutex_);
#else
            cond_.wait(lock, [=]() { return full && !drop_; });
#endif

            switch (drop_direction_) {
                case DROP_FRONT: {
                    if (!_can_drop(full, item)) {
                        if (item != nullptr)
                            queue_.push_back(std::move(item));
                        else
                            queue_.push_back(nullptr);
                    }
                    // else drop
                } break;
                case DROP_BACK: {
                    while (!queue_.empty() && _can_drop(full, queue_.front())) {
                        // discard old frames until we can discard
                        queue_.pop_front();
                        full = false;
                    }
                    queue_.push_back(std::move(item));
                } break;
                default: {
                    logger->warn("Wrong drop direction");
                } break;
            }
        }
#if __cplusplus <= 199711L
        pthread_cond_signal(&cond_);
        pthread_mutex_unlock(&mutex_);
#else
        cond_.notify_one();
#endif
    }

    void flush() {
        logger->debug("Flushing queue with {} items", size());

#if __cplusplus <= 199711L
        pthread_mutex_lock(&mutex_);
        flushing_ = true;
        pthread_cond_broadcast(&cond_);
        while (!queue_.empty())
            queue_.pop_front();
        pthread_mutex_unlock(&mutex_);
#else
        std::unique_lock<std::mutex> lock(mutex_);
        flushing_ = true;
        cond_.notify_all();
        while (!queue_.empty())
            queue_.pop_front();
#endif
    }

    void flush_stop() {
        logger->debug("Flushing queue stop with {} items", size());
        std::unique_lock<std::mutex> lock(mutex_);
        flushing_ = false;
        cond_.notify_all();
    }

    void set_eos(bool eos) {
        eos_ = eos;
        cond_.notify_one();
    }

    size_t size() {
        size_t size = 0;
#if __cplusplus <= 199711L
        pthread_mutex_lock(&mutex_);
        size = queue_.size();
        pthread_mutex_unlock(&mutex_);
#else
        std::unique_lock<std::mutex> lock(mutex_);
        size = queue_.size();
#endif
        return size;
    }

    size_t getMaxItems() { return max_items; }

protected:
    virtual bool _can_drop(bool full, const T& t) { return false; }

    bool wait_video_key_;
    bool drop_;
    DropDirection drop_direction_;
    std::deque<T> queue_;
    static constexpr int max_items = 90;

private:
#if __cplusplus <= 199711L
    pthread_mutex_t mutex_;
    pthread_cond_t cond_;
#else
    std::mutex mutex_;
    std::condition_variable cond_;
#endif
    bool flushing_ {false};
    bool eos_ {false};
};

//! @brief Stream info descriotion
struct StreamInfo {
    //! @brief Stream type
    enum StreamType { ST_UNKNOWN = -1, ST_VIDEO, ST_AUDIO, ST_DATA, ST_ANY };

    //! @brief Video codec type.
    enum VideoCodec {
        VC_UNKNOWN = -1,
        VC_H264,
        // TODO: more to add
    };

    //! @brief Video stream info. This structure as well as ISink::negotiate
    //! method aimed to inform sink about streams source provides, if sink
    //! don't care the values of this structure may be ignored.
    struct VideoInfo {
        //! @brief Video codec type.
        VideoCodec codec;
        //! @brief Video width if needed.
        int width;
        //! @brief Video height if needed.
        int height;
        //! @brief Video framerate if needed.
        std::pair<int, int> framerate;
        //! @brief Video bitrate if needed.
        int bitrate;
        //! @brief Timescale of the timestamps, source fills it with timescale
        //! of timestamps source receives, MediaFrame::pts should use this
        //! timescale.
        std::pair<int, int> timebase;
        //! @brief Can be AVC1 extradata or SPS/PPS, source fills it and sink
        //! should know and understand this format.
        std::vector<uint8_t> extradata;

        //! @private
        VideoInfo() : codec(VideoCodec::VC_UNKNOWN) {}
        //! @private
        ~VideoInfo() {}
    };

    //! @brief Audio codec
    enum AudioCodec {
        AC_UNKNOWN = -1,
        AC_AAC,
        AC_G711_U,
        AC_G711_A,
        AC_LPCM,
        AC_G726,
        AC_OPUS,

        // TODO: more to add
    };

    //! @brief Audio stream info.
    struct AudioInfo {
        //! @brief Audio codec
        AudioCodec codec;
        //! @brief Audio channels.
        int channels;
        //! @brief Audio samplerate.
        int samplerate;
        //! @brief Audio bitrate.
        int bitrate;
        //! @brief Audio timestamps timescale.
        std::pair<int, int> timebase;
        //! @brief Audio extradata. AAC requires one.
        std::vector<uint8_t> extradata;
        //! @private
        AudioInfo() : codec(AudioCodec::AC_UNKNOWN) {}
        //! @private
        ~AudioInfo() {}
    };

    //! @brief Data codec
    enum DataCodec {
        DC_UNKNOWN = -1,
        DC_ONVIF,
    };

    //! @private
    StreamInfo() : type(ST_UNKNOWN) {}
    //! @private
    ~StreamInfo() {}

    //! @brief Stream type
    StreamType type;
    //! @brief Video stream info. Should be filled if stream type is ST_VIDEO.
    VideoInfo video;
    //! @brief Audio stream info. Should be filled if stream type is ST_AUDIO.
    AudioInfo audio;
};

//! @brief Media frame type. Used to indicate when type of frame was passed from
//! source to sink.
enum MediaType {
    UKNOWN,
    VIDEO,
    VIDEO_AVC_SPS,
    VIDEO_AVC_PPS,
    VIDEO_SEQ_HDR,  // In case of when source has AVC1 extra data
    AUDIO,
    AUDIO_SEQ_HDR,
    FLV,
    DATA,
    MAX
};

//! @brief Media frame container
struct MediaFrame {
    //! @private
    MediaFrame() : data(0), len(0), pts(-1), duration(-1), is_key(false) {}
    //! @private
    MediaFrame(const MediaFrame& f) = default;
    //! @private
    MediaFrame& operator=(const MediaFrame&) = delete;

    //! @private
    ~MediaFrame() {}

    //!
    //! @brief Two frames comparation using timestamps.
    //!
    //! @param rv Right value
    //! @return true
    //! @return false
    //!
    bool operator<(const MediaFrame& rv) { return pts < rv.pts; }

    static constexpr int64_t NO_PTS = std::numeric_limits<int64_t>::min();

    //! Media frame data.
    std::vector<uint8_t> data;
    //! Media frame data length.
    size_t len;
    //! Media frame timestamp in timescale that corresponds to timescale.
    int64_t pts;
    //! Media frame decoding timestamp in timescale that corresponds to
    //! timescale.
    int64_t dts;
    //! Media frame duration if needed.
    int64_t duration;
    //! Is key frame flag.
    bool is_key;
    //! Media frame type.
    MediaType type;
    //! Timescale of pts and duration. ex. : 1/90000, 1/1000 etc.
    std::pair<int, int> timescale;
    //! Real time if available from source, for ex. pts based on NTP time from
    //! RTCP SR
    int64_t time_realtime {0};
};

//! @private
class MediaFrameQueue : public virtual Queue<std::shared_ptr<MediaFrame>> {
public:
    MediaFrameQueue(bool drop, DropDirection drop_direction = DROP_FRONT)
        : Queue<std::shared_ptr<MediaFrame>>(drop, drop_direction) {}

    virtual bool _can_drop(bool full,
                           const std::shared_ptr<MediaFrame>& frame) {
        bool can_drop = false;
        if (drop_) {
            // Video: drop any frame if queue is full and wait for key frame
            // Audio: drop any frame
            if (full || (_is_video(frame) && wait_video_key_)) {
                if (_is_video(frame)) {
                    if (!full) {
                        wait_video_key_ = !frame->is_key;
                        can_drop = !frame->is_key;
                    } else {
                        wait_video_key_ = true;
                        can_drop = true;
                    }
                    __vxg_stats_live_video_dropped += can_drop;

                    if (can_drop)
                        vxg::logger::instance("media-streaming-queue")
                            ->debug("Video frame dropped from media queue");
                } else if (full) {
                    __vxg_stats_live_audio_dropped++;
                    can_drop = true;
                }
            }
        }
        return can_drop;
    }

    bool _is_video(const std::shared_ptr<MediaFrame>& frame) {
        return frame &&
               (frame->type == VIDEO_AVC_SPS || frame->type == VIDEO_AVC_SPS ||
                frame->type == VIDEO_SEQ_HDR || frame->type == VIDEO);
    }
};

// @brief ISink interface class
//        Media stream sink interface, final class should be implemented with
//        specific library like ffmpeg or librtmp.
class ISink {
    vxg::logger::logger_ptr logger {vxg::logger::instance("media-sink")};

public:
    //! std::shared_ptr alias
    typedef std::shared_ptr<ISink> ptr;
    //! std::unique_ptr alias
    typedef std::unique_ptr<ISink> PtrU;

    //! @brief Construct a new ISink object
    //!
    //! @param prio internall thread priority, used on RTOS.
    ISink(uint8_t prio = SINK_THREAD_PRIO)
        : running_(false),
          q_(nullptr),
          thread_started_(false),
          thread_prio_(prio) {}

    virtual ~ISink() {
        if (thread_started_)
#if __cplusplus <= 199711L
            pthread_join(thr_, NULL);
#else
            if (thr_.joinable())
                thr_.join();
#endif
    }

    //! @brief Init sink
    //!
    //! @param[in] url Url if needed.
    //! @return true init success.
    //! @return false init failed.
    virtual bool init(std::string url = "") = 0;

    //! @brief Deinit sink
    //!
    //! @return true finit success.
    //! @return false finit failed.
    virtual bool finit() = 0;

    //! @brief Process next media frame.
    //!        Internal function called by media thread, the last function
    //!        of media frame travel.
    //!        Final class process frame in this function: sends to server,
    //!        writes on disk etc.
    //!
    //! @param[in] frame Media frame.
    //! @return true Media frame successfully processed.
    //! @return false Media frame processing failed.
    virtual bool process(std::shared_ptr<MediaFrame> frame) = 0;

    //! @brief If sink of with dropping its media frames.
    //!
    //! @return true Internal media thread allowed to drop frames if internal
    //!              media queue is full.
    //! @return false No media frames dropping allowed.
    virtual bool droppable() = 0;

    //! @brief Negotiation callback, this method called with collected from the
    //!        ISource::negotiate media stream description.
    //!
    //! @param info List of elementary streams descriptions.
    //! @return true If streams descriptions accepted.
    //! @return false Streams not accepted, will cause media thread stopping.
    virtual bool negotiate(std::vector<Streamer::StreamInfo> info) {
        return true;
    }

    //! @brief Media processing error callback, called when ISink::process
    //! returned false or linked source's ISource::pullFrame returned false,
    //! or when ISource::error was called.
    //! @param error Error type.
    virtual void error(StreamError error) = 0;

    //! @private
    void setQueue(std::shared_ptr<MediaFrameQueue> _q) { q_ = _q; }

    //! @private
    std::shared_ptr<MediaFrameQueue> getQueue() { return q_; }

    //! @private
    static void* _sink_routine(void* user_data) {
        ISink* sink = (ISink*)user_data;

        cloud::utils::set_thread_name(sink->name());

        sink->logger->debug("Routine started");

        while (sink->running_) {
            auto frame = sink->q_->pop();
            if (frame) {
                if (!sink->process(frame))
                    sink->error(E_FATAL);
            } else {
                if (sink->eos_cb_)
                    sink->eos_cb_();
                break;
            }
        }

        sink->logger->debug("Routine stopped");

        return NULL;
    }

    //! @brief Sink name.
    //!
    //! @return std::string
    virtual std::string name() = 0;

    //! @private
    virtual bool start() {
        if (running_) {
            logger->error("Trying to start {} which is already started!",
                          name());
            return false;
        }

        if (q_)
            running_ = true;

        struct sched_param sched = {
            .sched_priority = thread_prio_,
        };

#if __cplusplus <= 199711L
        pthread_attr_t tattr;
        pthread_attr_init(&tattr);
        pthread_attr_setstacksize(&tattr, 128 * 1024);
        pthread_attr_setschedparam(&tattr, &sched);

        if (0 == pthread_create(&thr_, &tattr, _sink_routine, this))
            thread_started_ = true;
#else
        thr_ = std::thread(&ISink::_sink_routine, this);
        thread_started_ = thr_.joinable();

        // TODO: C++11 doesn't have the way how to change stack size for
        // threads
        // Can we use it?
        pthread_setschedparam(thr_.native_handle(), SCHED_IDLE, &sched);
#endif

        return thread_started_;
    }

    //! @private
    virtual void stop() {
        running_ = false;
        if (q_)
            q_->flush();
        if (thread_started_) {
#if __cplusplus <= 199711L
            pthread_join(thr_, NULL);
#else
            if (thr_.get_id() != std::this_thread::get_id() && thr_.joinable())
                thr_.join();
            else
                thr_.detach();
#endif
            thread_started_ = false;
        }
    }

    //! @private
    size_t queueSize() { return q_->size(); }

    void set_eos_cb(std::function<void()> eos_cb) { eos_cb_ = eos_cb; }

    void set_eos(bool eos) { q_->set_eos(eos); }

private:
    std::atomic<bool> running_;
    std::shared_ptr<MediaFrameQueue> q_;
#if __cplusplus <= 199711L
    pthread_t thr_;
#else
    std::thread thr_;
#endif
    std::atomic<bool> thread_started_;
    uint8_t thread_prio_;
    bool eos_ {false};
    std::function<void()> eos_cb_ {nullptr};
};

//! @brief ISource interface class
// Media stream source interface, final class should be implemented with
// specific library like ffmpeg. Source interface provides two ways of data
// feeding, pull mode and push mode, in case of pull mode the
// ISource::pullFrame() will be called from the separate thread, push mode
// means that child class should call ISource::pushFrame() with MediaFrame
// object passed as argument.
class ISource {
    //! @private
    vxg::logger::logger_ptr logger {vxg::logger::instance("media-source")};

public:
    typedef std::shared_ptr<ISource> ptr;
    //! @brief Source operation mode.
    enum Mode {
        //! Pull mode. The ISource::pullFrame() will be called from the separate
        //! thread. User should implement it and return
        //! std::shared_ptr<MediaFrame>.
        PULL,
        //! Push mode. Inherited class should feed media data on its own by
        //! calling the ISource::pushFrame() method with MediaFrame object
        //! passed as argument.
        PUSH
    };

    //! @brief Construct a new ISource object
    //!
    //! @param[in] _prio Push thread priority. Used if \p _mode is Mode::PUSH.
    //! @param[in] _mode Source operating mode.
    //! @param[in] drop If true he media frames may be dropped if queue is full.
    ISource(uint8_t _prio = SRC_THREAD_PRIO,
            Mode _mode = PULL,
            bool drop = true)
        : running_(false),
          mode_(_mode),
          thread_started_(false),
          drop_(drop),
          wait_video_key_(false),
          thread_prio_(_prio) {
#if __cplusplus <= 199711L
        pthread_mutex_init(&multiqueue_lock_, NULL);
#endif
    }

    //! @private
    virtual ~ISource() {
#if __cplusplus <= 199711L
        pthread_mutex_destroy(&multiqueue_lock_);
#endif
    }

    //! @brief Init source.
    //!
    //! @param url Url if needed.
    //! @return true Init success.
    //! @return false Init failed.
    virtual bool init(std::string url = "") = 0;

    //! @brief Finit souce.
    virtual void finit() = 0;

    //! @brief Error notification. Calling this method will inform media thread
    //! and all sinks about error happened in the source.
    //!
    //! @param[in] stream_error
    virtual void error(StreamError stream_error) {
        switch (stream_error) {
            case Streamer::StreamError::E_NONE: {
            } break;
            case Streamer::StreamError::E_EOS: {
                logger->info("Source reported EOS, notifying all sinks");
                _errorAll(stream_error);
                running_ = false;
            } break;
            case Streamer::StreamError::E_FATAL: {
                logger->info(
                    "Source reported FATAL ERROR, all notifying sinks");
                _errorAll(stream_error);
                running_ = false;
            } break;
            default:
                logger->warn("Source reported unknown error {}", stream_error);
        }
    }

    //! @brief Negotiation callback. Called by internals. Class implementation
    //! should return the list of the streams info source will be producing for
    //! the sinks, this list will be then passed to the ISink::negotiate method.
    //!
    //! @return std::vector<Streamer::StreamInfo>
    virtual std::vector<Streamer::StreamInfo> negotiate() = 0;

    //! @brief Main method of the Mode::PULL mode data producing.
    //! Called by internals if the source operation mode is Mode::PULL.
    //! Implementation should return media frame object with correctly filled
    //! fields.
    //!
    //! @return std::shared_ptr<MediaFrame>
    virtual std::shared_ptr<MediaFrame> pullFrame() = 0;

    //! @private
    static void* _source_routine(void* user_data) {
        ISource* source = (ISource*)user_data;
        cloud::utils::set_thread_name(source->name());

        source->logger->debug("Source routine started");

        while (source->running_) {
            auto frame = source->pullFrame();
            if (frame) {
                source->_pushFrameAll(frame);
            } else {
                // source->error(E_EOS);
                // source->running_ = false;
            }
        }
        source->running_ = false;
        source->logger->debug("Source routine stopped");

        return NULL;
    }

    //! @brief Source class name
    //!
    //! @return std::string
    virtual std::string name() = 0;

    //! @private
    bool start() {
        if (running_) {
            logger->error("Trying to start {} which is already started!",
                          name());
            return false;
        }

        running_ = true;
        if (mode_ == PULL) {
            // Pull mode, separate thread pulls data with pullFrame()
            struct sched_param param;
            param.sched_priority = thread_prio_;

#if __cplusplus <= 199711L
            pthread_attr_t tattr;
            pthread_attr_init(&tattr);
            pthread_attr_setstacksize(&tattr, 128 * 1024);

            pthread_attr_setschedparam(&tattr, &param);

            if (0 == pthread_create(&thr_, &tattr, _source_routine, this))
                thread_started_ = true;
#else
            thr_ = std::thread(&ISource::_source_routine, this);
            pthread_setschedparam(thr_.native_handle(), SCHED_IDLE, &param);
            thread_started_ = thr_.joinable();
#endif
        } else {
            // Push mode, user will push frames with pushFrame()
        }
        return thread_started_;
    }

    //! @brief Implementation should call this method to provide media frames in
    //! the Mode::PUSH source operation mode.
    //!
    //! @param frame smart pointer to MediaFrame.
    void pushFrame(std::shared_ptr<MediaFrame> frame) {
#if __cplusplus <= 199711L
        pthread_mutex_lock(&multiqueue_lock_);
        _pushFrameAll(frame);
        pthread_mutex_unlock(&multiqueue_lock_);
#else
        const std::lock_guard<std::mutex> lock(multiqueue_lock_);
        _pushFrameAll(frame);
#endif
    }

    //! @private
    virtual void stop() {
        running_ = false;
        if (mode_ == PULL) {
            flush();

#if __cplusplus <= 199711L
            if (thread_started_)
                pthread_join(thr_, NULL);
#else

            if (thr_.get_id() != std::this_thread::get_id()) {
                if (thr_.joinable())
                    thr_.join();
            } else {
                try {
                    thr_.detach();
                } catch (const std::system_error& e) {
                    logger->warn("Unable to detach streaming thread: {}",
                                 e.what());
                }
            }
#endif
        }
    }

    //! @private
    void flush() {
#if __cplusplus <= 199711L
        pthread_mutex_lock(&multiqueue_lock_);
        _flushAll();
        pthread_mutex_unlock(&multiqueue_lock_);
#else
        const std::lock_guard<std::mutex> lock(multiqueue_lock_);
        _flushAll();
#endif
    }

    //! @private
    bool linkSink(ISink* sink) {
#if __cplusplus <= 199711L
        pthread_mutex_lock(&multiqueue_lock_);
#else
        const std::lock_guard<std::mutex> lock(multiqueue_lock_);
#endif
        if (sink) {
            logger->info("Link {}:@{} with source {}:@{}", sink->name(),
                         (void*)sink, name(), (void*)this);

            if (!sink->negotiate(negotiate())) {
                logger->error("{} => {} negotiation failed", name(),
                              sink->name());
                return false;
            }
            logger->info("{}:@{} negotiated with source {}:@{}", sink->name(),
                         (void*)sink, name(), (void*)this);

            sink_queue_map_[sink] =
                std::make_shared<MediaFrameQueue>(sink->droppable());
            sink->setQueue(sink_queue_map_[sink]);
        }

#if __cplusplus <= 199711L
        pthread_mutex_unlock(&multiqueue_lock_);
#endif
        return true;
    }

    //! @private
    bool unlinkSink(ISink* sink) {
#if __cplusplus <= 199711L
        pthread_mutex_lock(&multiqueue_lock_);
#else
        const std::lock_guard<std::mutex> lock(multiqueue_lock_);
#endif

        if (sink) {
            logger->debug("Unlink {} from source {}", sink->name(), name());
            sink_queue_map_.erase(sink);
        }

#if __cplusplus <= 199711L
        pthread_mutex_unlock(&multiqueue_lock_);
#endif
        return true;
    }

    //! @private
    bool is_running() { return running_; }

private:
    void __update_sink_queue_stats(ISink* sink) {
        if (strstr(sink->name().c_str(), "rtmp-sink"))
            __vxg_stats_live_queue = sink->getQueue()->size();
    }

    void _pushFrameAll(std::shared_ptr<MediaFrame> frame) {
        const std::lock_guard<std::mutex> lock(multiqueue_lock_);

        /* Copy frame pointer to all connected sinks */
        for (const auto& kv : sink_queue_map_) {
            kv.second->push(frame);
            __update_sink_queue_stats(kv.first);
        }
    }

    void _errorAll(StreamError error) {
        // Client code may unlink and remove sink from mq on error,
        // use copy here
        std::map<ISink*, std::shared_ptr<MediaFrameQueue>> multiqueue_copy =
            sink_queue_map_;
        /* notify error to all connected sinks */
        for (const auto& kv : multiqueue_copy) {
            kv.first->error(error);
        }
    }

    void _flushAll() {
        /* Flush queues in all connected sinks */
        for (const auto& kv : sink_queue_map_) {
            kv.second->flush();
            __update_sink_queue_stats(kv.first);
        }
    }

protected:
    Mode mode_;

private:
    bool running_;
    std::map<ISink*, std::shared_ptr<MediaFrameQueue>> sink_queue_map_;
#if __cplusplus <= 199711L
    pthread_t thr_;
    pthread_mutex_t multiqueue_lock_;
#else
    std::thread thr_;
    std::mutex multiqueue_lock_;
#endif
    uint8_t thread_prio_;
    bool thread_started_;
    static uint32_t sink_id_;

    bool drop_;
    bool wait_video_key_;
};
}  // namespace Streamer
}  // namespace media
}  // namespace vxg
#endif