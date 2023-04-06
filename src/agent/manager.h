#pragma once

#include <agent-proto/command-handler.h>
#include <agent/callback.h>
#include <agent/event-stream.h>
#include <agent/manager-config.h>
#include <cloud/CloudShareConnection.h>

#include <agent/stream.h>
#include <agent/upload.h>
#include <net/http.h>
#include <utils/logging.h>

namespace vxg {
namespace cloud {
//! @brief VXG Cloud Agent namespace
namespace agent {

//! @brief VXG Cloud Agent library version
//!
//! @return std::string version string
//!
std::string version();

//!
//! @brief VXG Cloud agent manager class
//!
class manager : public proto::command_handler {
    vxg::logger::logger_ptr logger = vxg::logger::instance("agent-manager");

private:
    bool started_;
    std::string cloud_token_;
    std::string svcp_url_;
    CloudShareConnection cloud_shared_connection_;
    proto::events_config events_config_;
    std::map<int, proto::event_object::snapshot_info_object> snapshots_;
    std::map<int, proto::video_clip_info> records_;
    std::map<int, proto::event_object::file_meta_info_object> meta_files_;
    transport::libwebsockets::http::ptr http_;
    transport::libwebsockets::http::ptr direct_upload_timer_;
    std::vector<std::shared_ptr<agent::media::stream>> streams_;
    std::vector<std::shared_ptr<event_stream>> event_streams_;
    int active_sinks_ {0};
    int record_event_depth_ {0};
    bool record_by_event_ {false};
    bool stream_by_event_ {false};
    bool continuos_direct_record_ {false};
    std::chrono::milliseconds pre_record_time_ {
        profile::global::instance().default_pre_record_time};
    std::chrono::milliseconds post_record_time_ {
        profile::global::instance().default_post_record_time};
    agent::media::stream::ptr record_stream_ {nullptr};
    agent::media::stream::ptr live_stream_ {nullptr};
    agent::media::stream::ptr snapshot_stream_ {nullptr};
    std::atomic<bool> preview_uploading {false};
    transport::timed_cb_ptr timed_cb_direct_uploader_;
    proto::event_object::memorycard_info_object memorycard_info_;
    callback::ptr callback_ {nullptr};
    std::deque<proto::get_direct_upload_url> direct_upload_video_q_;
    std::deque<proto::get_direct_upload_url> direct_upload_snapshot_q_;
    std::map<std::string, transport::timed_cb_ptr> periodic_events_;
    agent::segmented_uploader::ptr uploader_;

    struct event_state {
        vxg::logger::logger_ptr logger = vxg::logger::instance("event-state");

        cloud::time start_ {utils::time::null()};
        cloud::time stop_ {utils::time::max()};

        bool active_ {false};
        struct event_state_caps {
            bool stateful {false};
            bool need_clip {false};
            bool need_snapshot {false};
            bool internal_hidden {false};
        };
        event_state_caps caps_;
        proto::event_object event_kick_;
        proto::event_config event_conf_;
        const int MAX_EVENT_KICK_INTERVAL = 10;
        const cloud::duration event_kick_period_ {
            std::chrono::seconds(MAX_EVENT_KICK_INTERVAL)};
        using event_notify_cb = std::function<bool(proto::event_object)>;
        event_notify_cb event_notify_cb_ {nullptr};

        using segmenter_ptr = segmented_uploader::segmenter::ptr;
        segmenter_ptr segmenter_ {nullptr};
        transport::timed_cb_ptr stateful_periodic_kicker_;
        transport::timed_cb_ptr stream_delivery_stop_pending_timer_;
        transport::libwebsockets::http::ptr transport_ {nullptr};
        segmented_uploader::ptr uploader_;

        const cloud::duration RECORDS_UPLOAD_START_DELAY =
            profile::global::instance()
                .delay_between_event_and_records_upload_start;

    public:
        enum stream_delivery_mode { SDM_NONE, SDM_UPLOAD, SDM_STREAM };

    private:
        stream_delivery_mode delivery_mode_ {SDM_NONE};
        std::function<void(std::string name, proto::stream_reason)>
            start_stream_cb_;
        std::function<void(std::string name, proto::stream_reason)>
            stop_stream_cb_;
        std::chrono::milliseconds pre_record_time_ {std::chrono::seconds(5)};
        std::chrono::milliseconds post_record_time_ {std::chrono::seconds(5)};

        ///! @internal
        ///! Stateful events emulation
        ///! Sending dummy event that follows up original event state change
        ///! every event_kick_period_ seconds until event state changed to
        ///! false.
        void _event_kick() {
            // Adjust event time by constant period
            event_kick_.time = utils::time::to_double(
                utils::time::from_double(event_kick_.time) +
                event_kick_period_);
            event_kick_.status = proto::ES_OK;
            event_kick_.state_dummy = true;

            // Schedule next kick
            stateful_periodic_kicker_ = transport_->schedule_timed_cb(
                std::bind(&event_state::_event_kick, this),
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    event_kick_period_)
                    .count());
            logger->trace("Periodic dummy event kicker {} shedulled in {} ms",
                          (void*)stateful_periodic_kicker_.get(),
                          std::chrono::duration_cast<std::chrono::milliseconds>(
                              event_kick_period_)
                              .count());

            if (event_notify_cb_)
                event_notify_cb_(event_kick_);
        }
        //! @endinternal

        void _start_periodic_event_kicker() {
            // Schedule first kick
            stateful_periodic_kicker_ = transport_->schedule_timed_cb(
                std::bind(&event_state::_event_kick, this),
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    event_kick_period_)
                    .count());
            logger->trace("Periodic dummy event kicker {} started",
                          (void*)stateful_periodic_kicker_.get());
        }

        void _stop_periodic_event_kicker() {
            logger->trace("Periodic dummy event kicker {} stopping",
                          (void*)stateful_periodic_kicker_.get());
            if (stateful_periodic_kicker_) {
                transport_->cancel_timed_cb(stateful_periodic_kicker_);
                // stateful_periodic_kicker_ = nullptr;
            }
        }

        void _start_upload_delivery() {
            // Start upload for created segmenter.
            // Passing segmenter by the value will ref the shared_ptr.
            uploader_->schedule_upload(segmenter_);
        }

        void _stop_upload_delivery() {
            if (stop_ == utils::time::null())
                stop_ = utils::time::now();
            // Schedule chunker finalization, lock-free because
            // scheduled functions are serialized
            uploader_->finalize_upload(segmenter_, stop_);
        }

        void _start_stream_delivery() {
            if (start_stream_cb_)
                transport_->schedule_timed_cb(
                    std::bind(start_stream_cb_,
                              segmenter_->seg_provider->cloud_name(),
                              proto::stream_reason::SR_RECORD),
                    0);
        }

        void _stop_stream_delivery() {
            using namespace std::chrono;

            // Stopping stream actually stop stream only if number_of_starts ==
            // number_of_stops, this rule should handle intersecting pre/post
            // time of the events
            if (stop_stream_cb_ && segmenter_ && segmenter_->seg_provider)
                stream_delivery_stop_pending_timer_ =
                    transport_->schedule_timed_cb(
                        [&]() {
                            stop_stream_cb_(
                                segmenter_->seg_provider->cloud_name(),
                                proto::stream_reason::SR_RECORD);
                            stream_delivery_stop_pending_timer_ = nullptr;
                        },
                        post_record_time_.count());
        }

        void _start_delivery() {
            switch (delivery_mode_) {
                case SDM_UPLOAD: {
                    _start_upload_delivery();
                } break;
                case SDM_STREAM: {
                    _start_stream_delivery();
                } break;
                case SDM_NONE:
                default:
                    break;
            }
        }

        void _stop_delivery() {
            switch (delivery_mode_) {
                case SDM_UPLOAD:
                    _stop_upload_delivery();
                    break;
                case SDM_STREAM: {
                    _stop_stream_delivery();
                } break;
                case SDM_NONE: {
                } break;
                default:
                    break;
            }
        }
        ///! @endinternal

    public:
        event_state() { segment_reset(); }

        event_state(const proto::event_config event_conf,
                    transport::libwebsockets::http::ptr transport,
                    event_notify_cb event_cb,
                    segmented_uploader::ptr uploader = nullptr,
                    std::function<void(std::string name, proto::stream_reason)>
                        start_stream_cb = nullptr,
                    std::function<void(std::string name, proto::stream_reason)>
                        stop_stream_cb = nullptr)
            :  // Records upload, should be started outside
              uploader_ {uploader},
              // Statefull event activity emulation
              transport_ {transport},
              event_notify_cb_ {event_cb},
              // Stream start/stop callbacks for SDM_STREAM delivery mode
              start_stream_cb_ {start_stream_cb},
              stop_stream_cb_ {stop_stream_cb},
              event_conf_ {event_conf} {
            caps_.stateful = event_conf.caps.statefull;
            caps_.need_clip = event_conf.caps.stream;
            caps_.need_snapshot = event_conf.caps.snapshot;
            caps_.internal_hidden = event_conf.caps.internal_hidden;

            segment_reset();
        }

        ~event_state() {
            if (event_conf_.event == proto::ET_INVALID)
                return;

            logger->info("Event state {} destroying",
                         json(event_conf_)["event"]);

            // Stop periodic event kicker before destructing
            if (caps_.stateful)
                _stop_periodic_event_kicker();

            if (start_ != utils::time::null() && caps_.stateful &&
                stop_ == utils::time::null()) {
                stop_ = utils::time::now();

                logger->info(
                    "Event {} statefull ongoing event force stopped at {}",
                    json(event_conf_)["event"], utils::time::to_iso(stop_));
            }
            _stop_delivery();
        }

        event_state(const event_state& r) {
            caps_ = r.caps_;
            event_notify_cb_ = r.event_notify_cb_;
            transport_ = r.transport_;
            uploader_ = r.uploader_;
            start_stream_cb_ = r.start_stream_cb_;
            stop_stream_cb_ = r.stop_stream_cb_;
            event_conf_ = r.event_conf_;
        }

        friend void swap(event_state& l, event_state& r) {
            std::swap(l.stateful_periodic_kicker_, r.stateful_periodic_kicker_);
            std::swap(l.segmenter_, r.segmenter_);
            std::swap(l.transport_, r.transport_);
            std::swap(l.uploader_, r.uploader_);
            std::swap(l.start_stream_cb_, r.start_stream_cb_);
            std::swap(l.stop_stream_cb_, r.stop_stream_cb_);
            std::swap(l.caps_, r.caps_);
            std::swap(l.event_notify_cb_, r.event_notify_cb_);
            std::swap(l.event_conf_, r.event_conf_);
        }

        // Arg passed by value, i.e. copy constructor called first for following
        // copy-swap idiom
        event_state& operator=(event_state r) noexcept {
            // friend swap implementation
            swap(*this, r);
            return *this;
        }

        void start(stream_delivery_mode delivery_mode,
                   proto::event_object event,
                   cloud::duration pre,
                   cloud::duration post,
                   agent::media::stream::ptr seg_provider = nullptr,
                   std::string upload_token = "") {
            // Give some time for device to create recordings by delaying export
            cloud::duration delay = RECORDS_UPLOAD_START_DELAY;
            bool realtime = true;

            // Statefull event
            if (caps_.stateful) {
                active_ = true;
                start_ = utils::time::from_double(event.time);
                stop_ = utils::time::null();

                event_kick_.event = event.event;
                event_kick_.custom_event_name = event.custom_event_name;
                event_kick_.time = event.time;

                if (!caps_.internal_hidden)
                    _start_periodic_event_kicker();
            } else {
                // Stateless event
                start_ = stop_ = utils::time::from_double(event.time);

                // Extended clips upload time for stateless events if user code
                // provided event's time_end, this will also disables realtime
                // upload i.e. segmenter will not be waiting for the next chunk
                // during delay time.
                if (utils::time::from_double(event.time_end) !=
                    utils::time::null()) {
                    stop_ = utils::time::from_double(event.time_end);
                    delay = chrono::seconds(0);
                    realtime = false;
                }
            }

            delivery_mode_ = delivery_mode;
            pre_record_time_ =
                std::chrono::duration_cast<std::chrono::milliseconds>(pre);
            post_record_time_ =
                std::chrono::duration_cast<std::chrono::milliseconds>(post);

            // For the statefull event:
            //  Create chunker, start time is event start
            //  Stop time will be filled on event stop
            //  We use 2 functions for uploading recordings chunked:
            //   - schedule_upload() scheduled to upload one chunk of
            //     the recording, if it's not the last chunk this
            //     function scheduled again with the new start time
            //     which is prev chunk stop time. Last chunk is the
            //     chunk with a stop time equal to the chunker stop
            //     time.
            //   - finalize_upload() scheduled when
            //     the event was finished, fills in chunker stop time.
            //  Scheduling functions with LWS gives serialization and
            //  no need to use locks
            //
            // For the stateless event:
            //  stop = start, or stop = event.end_time if specified by user
            segment_reset();

            segmenter_->begin = start_;
            segmenter_->end = stop_;
            segmenter_->cur_seg_start = start_ - pre;
            segmenter_->step =
                profile::global::instance().record_by_event_upload_step;
            segmenter_->cur_seg_stop =
                std::min(start_ + segmenter_->step, stop_ != utils::time::null()
                                                        ? stop_ + post
                                                        : utils::time::max());
            segmenter_->processed = false;
            segmenter_->pre = pre;
            segmenter_->post = post;
            segmenter_->seg_provider = seg_provider;
            segmenter_->ticket = upload_token;
            segmenter_->realtime = realtime;
            segmenter_->delay = delay;
            segmenter_->last_processed_time = utils::time::now();

            logger->debug(
                "New segmenter {} - {}, cur_seg {} - {}, pre: {}, post: {}, "
                "delay {}",
                utils::time::to_iso(segmenter_->begin),
                utils::time::to_iso(segmenter_->end),
                utils::time::to_iso(segmenter_->cur_seg_start),
                utils::time::to_iso(segmenter_->cur_seg_stop),
                segmenter_->pre.count(), segmenter_->post.count(),
                segmenter_->delay.count());

            _start_delivery();
        }

        void stop(cloud::time time) {
            if (caps_.stateful) {
                stop_ = time;
                active_ = false;
                if (!caps_.internal_hidden)
                    _stop_periodic_event_kicker();
            }

            if (segmenter_->end == utils::time::null())
                segmenter_->end = time;

            _stop_delivery();
        }

        // Currently used only to turn on the record upload/stream when by_event
        // was started after the event was emitted.
        void update_delivery_mode(
            stream_delivery_mode delivery_mode,
            agent::media::stream::ptr seg_provider = nullptr) {
            if (delivery_mode != delivery_mode_) {
                _stop_delivery();
                delivery_mode_ = delivery_mode;
                segmenter_->seg_provider = seg_provider;
                segmenter_->begin = utils::time::now();
                segmenter_->cur_seg_start = segmenter_->begin;

                if (stream_delivery_stop_pending_timer_) {
                    transport_->cancel_timed_cb(
                        stream_delivery_stop_pending_timer_);
                    stream_delivery_stop_pending_timer_ = nullptr;
                }

                _start_delivery();
            }
        }

        bool active() { return active_; }
        bool stateful() { return caps_.stateful; }
        bool need_record() { return caps_.need_clip; }
        bool post_stop_pending() {
            return !!stream_delivery_stop_pending_timer_;
        }
        cloud::time start() { return start_; }
        cloud::time stop() { return stop_; }
        segmenter_ptr segment() { return segmenter_; }

        void segment_reset() {
            segmenter_ = std::make_shared<segmented_uploader::segmenter>();
        }
    };
    // map[event name, event state]
    std::map<std::string, event_state> events_states_;
    // map[event stream name, event configs list]
    std::map<std::string, std::vector<proto::event_config>>
        event_streams_configs_;

    struct recording_status {
        bool permanent {false};
        bool by_event {false};
        bool direct_upload {false};
        bool started {false};
        std::string stream_id;
        proto::stream_config config;
        cloud::time last_start_time;
        size_t external_stop_count;

        bool enabled() { return (permanent || by_event || direct_upload); }
        bool need_start() {
            return (permanent || by_event || direct_upload) && !started;
        }
    };
    std::map<media::stream::ptr, recording_status> recording_status_;
    bool _start_recording(media::stream::ptr stream);
    bool _stop_recording(media::stream::ptr stream, bool force = false);

public:
    //! \private
    virtual ~manager();

    //! @brief shared_ptr to manager object
    typedef std::shared_ptr<manager> ptr;
    //!
    //! @brief Create manager object
    //!
    //! @param[in] callback cm::callback object, should not be null
    //! @param[in] access_token VXG Cloud access token
    //! @param[in] media_streams List of std::shared_ptr to base_stream derived
    //!        objects. Should have at least one element.
    //!        base_stream is abstract class so you need
    //!        to declare you own class derived from the base_stream or use
    //!        one of the provided classes (rtsp_stream,...), basically each
    //!        stream is for example one rtsp stream provided by the device.
    //!        Each media stream device has should be represented as a separate
    //!        base_stream derived object, currently only two streams per device
    //!        are supported by the VXG Cloud.
    //! @param[in] event_streams List of event_stream::ptr, can be empty.
    //!        event_stream is abstract class so final implementation should
    //!        use own class derived from the event_stream.
    //! @return manager::ptr
    //!
    static manager::ptr create(
        callback::ptr callback,
        proto::access_token::ptr access_token,
        std::vector<agent::media::stream::ptr> media_streams,
        std::vector<event_stream::ptr> event_streams =
            std::vector<event_stream::ptr>(0));

    //!
    //! @brief Start internal workflow, this is the main function which starts
    //!        all internal threads and connections
    //!
    //! @return true started
    //! @return false start failed
    //!
    bool start();
    //!
    //! @brief Stop manager, disconnect from the VXG Cloud.
    //!
    //!
    void stop();

protected:
    //! @brief Construct a new manager object
    //!
    //! @param[in] callback
    //! @param[in] transport Used as protocol transport if specified, if nullptr
    //! the library will create own websockets transport.
    //! @private
    manager(callback::ptr callback, transport::worker::ptr transport = nullptr);

    //! \todo Use proto::access_token as argument
    //! @brief Set the VXG Cloud streaming access token
    //!        Token should be set before starting the manager with start()
    //!
    //! @param[in] access_token Base64 encoded access token
    //! @return true Token validated and was set
    //! @return false Token is invalid
    //! @private
    bool set_token(const std::string access_token);

    //! @private
    bool set_token(const proto::access_token access_token);

    //!
    //! @brief Set base_stream list, base_stream is abstract class so you need
    //!        to declare you own class derived from the base_stream or use
    //!        one of the provided classes (rtsp_stream,...), basically each
    //!        stream is for example one rtsp stream provided by the device.
    //!        Each media stream device has should be represented as a separate
    //!        base_stream derived object, currently only one stream per device
    //!        is supported by the VXG Cloud.
    //!
    //! @param[in] streams list of media streams supported by device
    //! @private
    void set_streams(std::vector<agent::media::stream::ptr> streams);
    //!
    //! @brief Set event_stream list, event_stream is abstract class so final
    //!        implementation should use own class derived from the event_stream
    //!
    //! @param[in] streams
    //! @private
    void set_event_streams(std::vector<event_stream::ptr> streams);

    bool notify_event(proto::event_object event);

    // Storage status event trigger
    bool _update_storage_status();

    //! Streams helpers
    void _stop_all_streams(bool sync = false);
    void _stop_stream(agent::media::stream::ptr s, bool sync = false);
    void _stop_all_event_streams();

    // Periodic events internals
    void _schedule_periodic_events(proto::events_config& events_conf);
    void _schedule_periodic_event(const proto::event_config& event_conf);
    void _cancel_periodic_event(const proto::event_config& event_conf);
    void _cancel_periodic_events(const proto::events_config& events_conf);
    void _append_internal_custom_events(proto::events_config& config);
    void __trigger_periodic_event(const proto::event_config& event_conf);

    // Events
    void _init_events_states(const proto::events_config& config);
    void _load_events_configs(proto::events_config& config);
    bool _update_events_configs(
        const std::vector<proto::event_config>& new_configs,
        std::vector<proto::event_config>& dest_configs);
    bool _update_event_stream_configs(
        const std::string& stream_name,
        const std::vector<proto::event_config>& new_configs);
    event_stream::ptr _lookup_event_stream_by_event(
        const proto::event_object& event);
    event_stream::ptr _lookup_event_stream(const std::string& name);

    bool handle_stream_event(proto::event_object& event);
    bool _handle_stream_stateful_event(
        proto::event_object& event,
        event_state::stream_delivery_mode delivery_mode);
    bool _handle_stream_stateless_event(
        proto::event_object& event,
        event_state::stream_delivery_mode delivery_mode);
    bool handle_event_snapshot(proto::event_object& event);
    bool handle_event_meta_file(proto::event_object& event);
    bool __notify_record_event(std::string stream_id, bool on);
    event_state::stream_delivery_mode _current_delivery_mode();

    // Upload
    bool _schedule_direct_upload(proto::get_direct_upload_url get_upload_url);
    bool _cancel_direct_uploads_by_ticket(std::string ticket);
    bool _request_direct_upload_video(
        proto::get_direct_upload_url direct_upload);
    bool _request_direct_upload_snapshot(
        proto::get_direct_upload_url direct_upload);
    void _update_direct_upload_queue_latency();
    bool direct_upload_sync_cb();

    agent::media::stream::ptr lookup_stream(std::string name);

    // Proto callbacks
    virtual bool on_get_stream_config(proto::stream_config& config);
    virtual bool on_set_stream_config(const proto::stream_config& config);

    virtual bool on_get_motion_detection_config(
        proto::motion_detection_config& config);
    virtual bool on_set_motion_detection_config(
        const proto::motion_detection_config& config);

    virtual bool on_get_cam_video_config(proto::video_config& config);
    virtual bool on_set_cam_video_config(const proto::video_config& config);

    virtual bool on_get_cam_events_config(proto::events_config& config);
    virtual bool on_set_cam_events_config(const proto::events_config& config);

    virtual bool on_get_cam_audio_config(proto::audio_config& config);
    virtual bool on_set_cam_audio_config(const proto::audio_config& config);

    virtual bool on_get_ptz_config(proto::ptz_config& config);
    virtual bool on_cam_ptz(proto::ptz_command command);
    virtual bool on_cam_ptz_preset(proto::ptz_preset& preset_op);

    virtual bool on_get_osd_config(proto::osd_config& config);
    virtual bool on_set_osd_config(const proto::osd_config& config);

    virtual bool on_get_wifi_config(proto::wifi_config& config);
    virtual bool on_set_wifi_config(const proto::wifi_network& config);

    virtual bool on_stream_start(const std::string& streamId,
                                 int publishSessionID,
                                 proto::stream_reason reason);
    virtual bool on_stream_stop(const std::string& streamId,
                                proto::stream_reason reason);

    virtual bool on_get_stream_caps(proto::stream_caps& caps);

    virtual bool on_get_supported_streams(
        proto::supported_streams_config& supportedStreamsConfig);

    virtual bool on_cam_upgrade_firmware(std::string url);
    virtual bool on_raw_message(std::string client_id, std::string& data);

    virtual bool on_set_stream_by_event(proto::stream_by_event_config conf);
    virtual bool on_get_stream_by_event(proto::stream_by_event_config& conf);

    virtual bool on_update_preview(std::string url);

    virtual bool on_direct_upload_url(
        const proto::command::direct_upload_url_base& direct_upload,
        int event_id,
        int ref_id);
    virtual bool on_get_log();

    virtual void on_prepared();
    virtual void on_closed(int error, proto::command::bye_reason reason);

    virtual bool on_get_timezone(std::string& timezone);
    virtual bool on_set_timezone(std::string timezone);
    void on_set_periodic_events(const char* name, int period, bool active);

    // Two-way audio backward channel
    virtual bool on_audio_file_play(std::string url);
    virtual bool on_start_backward(std::string& url);
    virtual bool on_stop_backward(std::string& url);

    // SD recording synchronization
    virtual bool on_get_cam_memorycard_timeline(
        proto::command::cam_memorycard_timeline& timeline);
    virtual bool on_cam_memorycard_synchronize(
        proto::command::cam_memorycard_synchronize_status& synchronize_status,
        vxg::cloud::time start,
        vxg::cloud::time end);
    virtual bool on_cam_memorycard_synchronize_cancel(
        const std::string& request_id);
    virtual bool on_cam_memorycard_recording(const std::string& stream_id,
                                             bool enabled);

    virtual bool on_trigger_event(std::string event,
                                  json meta,
                                  cloud::time time);

    virtual bool on_set_audio_detection(
        const proto::audio_detection_config& conf);
    virtual bool on_get_audio_detection(proto::audio_detection_config& conf);

    // TODO:
    virtual bool on_set_log_enable(bool bEnable);
    virtual bool on_set_activity(bool bEnable);

    virtual void on_registered(const std::string& sid);
};
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
