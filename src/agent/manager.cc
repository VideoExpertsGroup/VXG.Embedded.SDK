#include <nlohmann/json.hpp>

#include <agent/manager.h>
#include <agent/upload.h>
#include <net/websockets.h>
#include <utils/base64.h>
#include <utils/utils.h>

#include <config.h>

using json = nlohmann::json;

namespace vxg {
namespace cloud {

using request = transport::libwebsockets::http::request;
using http = transport::libwebsockets::http;
using websocket = transport::libwebsockets::websocket;
using stream = agent::media::stream;

namespace agent {
std::string version() {
    return std::string(VXGCLOUDAGENT_LIB_VERSION);
}

using namespace proto::command;

manager::manager(callback::ptr callback, transport::worker::ptr transport)
    : command_handler(transport),
      callback_(std::move(callback)),
      started_(false),
      http_ {std::make_shared<transport::libwebsockets::http>(
          profile::global::instance().allow_invalid_ssl_certs)},
      direct_upload_timer_ {
          std::make_shared<transport::libwebsockets::http>()} {
    if (!callback_)
        throw std::runtime_error("Callback should not be null!!!");

    registered_ = false;
    hello_received_ = false;
    auto_reconnect_ = true;

    // FIXME: probably move somewhere else
    events_config_.events.clear();
}

manager::~manager() {
    stop();
}

static void __normalize_access_token(proto::access_token& access_token) {
    // Default values if some fields are unset
    // If no cam host we should use api host as cam
    if (__is_unset(access_token.api))
        access_token.api = DEFAULT_CLOUD_TOKEN_API;
    else if (__is_unset(access_token.cam))
        access_token.cam = access_token.api;
    if (__is_unset(access_token.api_p))
        access_token.api_p = DEFAULT_CLOUD_TOKEN_API_P;
    if (__is_unset(access_token.api_sp))
        access_token.api_sp = DEFAULT_CLOUD_TOKEN_API_SP;
    if (__is_unset(access_token.cam))
        access_token.cam = DEFAULT_CLOUD_TOKEN_CAM;
    if (__is_unset(access_token.cam_p))
        access_token.cam_p = DEFAULT_CLOUD_TOKEN_CAM_P;
    if (__is_unset(access_token.cam_sp))
        access_token.cam_sp = DEFAULT_CLOUD_TOKEN_CAM_SP;
}

manager::ptr manager::create(callback::ptr callback,
                             proto::access_token::ptr access_token,
                             std::vector<stream::ptr> media_streams,
                             std::vector<event_stream::ptr> event_streams) {
    if (callback == nullptr) {
        vxg::logger::error("Callback should not be nullptr");
        return nullptr;
    }

    struct make_shared_enabler : public manager {
        make_shared_enabler(callback::ptr c) : manager(std::move(c)) {}
    };
    auto result = std::make_shared<make_shared_enabler>(std::move(callback));
    if (result != nullptr) {
        if (access_token == nullptr) {
            vxg::logger::error("Failed to set nullptr access token");
            return nullptr;
        }

        __normalize_access_token(*access_token);

        if (!result->set_token(*access_token)) {
            vxg::logger::error("Failed to set access token");
            return nullptr;
        }

        result->set_streams(media_streams);

        result->set_event_streams(event_streams);

        return result;
    }

    vxg::logger::error("Failed to create manager");

    return nullptr;
}

void manager::set_streams(std::vector<stream::ptr> streams) {
    streams_ = streams;

    if (!streams_.empty()) {
        snapshot_stream_ = streams_[0];
        record_stream_ = streams_[0];
        logger->info("Using stream {} for snapshots and record by default",
                     snapshot_stream_->cloud_name());
    } else {
        logger->warn(
            "No media streams were provided by user, working in an idle mode!");
    }
}

void manager::set_event_streams(std::vector<event_stream::ptr> streams) {
    event_streams_ = streams;

    if (event_streams_.empty())
        logger->warn(
            "No event streams were provided by user, events and recording by "
            "event will not be working!");
}

bool manager::set_token(const std::string packed_token) {
    proto::access_token access_token;

    try {
        std::string decoded_token = base64_decode(packed_token);
        access_token = json::parse(decoded_token);

        __normalize_access_token(access_token);
    } catch (const char* str) {
        logger->error("Unable to decode base64 token");
        return false;
    } catch (const std::exception& e) {
        logger->error("Unable to parse source token");
        return false;
    }

    return set_token(access_token);
}

// TODO: refactor this legacy mess
bool manager::set_token(const proto::access_token access_token) {
    uint16_t svcp_port = 8888;
    std::string proto = "http://";
    std::string host;
    std::string token;

    if (access_token.cam.empty())
        host = access_token.api;
    else
        host = access_token.cam;

    if (!host.empty()) {
        // FIXME: remove
        if (!profile::global::instance().insecure_cloud_channel) {
            if (access_token.cam_sp > 0)
                svcp_port = access_token.cam_sp;
            else
                svcp_port = 8883;

            proto = "https://";
        } else {
            if (access_token.cam_p > 0)
                svcp_port = access_token.cam_p;
            else
                svcp_port = 8888;

            proto = "http://";
        }
    }

    if (!host.empty())
        svcp_url_ = proto + host + ":" + std::to_string(svcp_port);
    else {
        Uri uri = Uri::Parse(access_token.svcp);
        svcp_url_ = "http://" + uri.Host + ":8888";
    }

    logger->info("SVCP: {}", svcp_url_.c_str());

    // open connection
    if (svcp_url_.empty()) {
        cloud_shared_connection_.openSync(cloud_token_);
    } else {
        cloud_shared_connection_.openSync(cloud_token_, svcp_url_);
    }

    CloudRegToken cloudToken(
        std::string("{\"token\" : \"" + access_token.token + "\"}"));
    cm_config_.reg_token = cloudToken;
    cm_config_.set_cm_address(
        cloud_shared_connection_._getCloudAPI().getCMAddress());

    if (!__is_unset(access_token.proxy.socks5)) {
        logger->info("Access token contains sock5 proxy {}",
                     access_token.proxy.socks5);
        cm_config_.proxy_socks5_uri_ = access_token.proxy.socks5;
    }

    if (!__is_unset(access_token.proxy.socks4)) {
        logger->info("Access token contains socks4 proxy {}",
                     access_token.proxy.socks4);
        cm_config_.proxy_socks4_uri_ = access_token.proxy.socks4;
    }

    cm_config_.access_token_ = access_token;

    return true;
}

bool manager::start() {
    websocket::ptr ws = dynamic_pointer_cast<websocket>(transport_);
    // Bind callbacks, bind gives no need to pass a userdata
    ws->set_connected_cb(std::bind(&command_handler::on_connected, this));
    ws->set_disconnected_cb(std::bind(&command_handler::on_disconnected, this));
    ws->set_error_cb(std::bind(&command_handler::on_error, this,
                               placeholders::_1, placeholders::_2));

    if (!cm_config_.proxy_socks5_uri_.empty()) {
        ws->set_proxy(cm_config_.proxy_socks5_uri_);
        http_->set_proxy(cm_config_.proxy_socks5_uri_);
    }

    // Start libwebsockets service threads for HTTP and WebSockets
    http_->start();
    transport_->start();
    direct_upload_timer_->start();

    // Connection to the cam websocket server, connection attempt failure will
    // be handled by command_proto::on_error and command_proto::on_disconnect
    try_connect();

    timed_cb_direct_uploader_ = direct_upload_timer_->schedule_timed_cb(
        [=] { direct_upload_sync_cb(); }, 10000);

    uploader_ = segmented_uploader::create(
        std::make_shared<proto::access_token>(cm_config_.access_token_),
        nullptr,
        std::bind(&manager::_schedule_direct_upload, this, placeholders::_1),
        [this](const json& msg) { return send_command(msg); });
    uploader_->start();

    return true;
}

void manager::stop() {
    hello_received_ = false;
    registered_ = false;

    _stop_all_event_streams();

    if (uploader_)
        uploader_->stop();

    if (transport_) {
        if (timed_cb_direct_uploader_) {
            direct_upload_timer_->cancel_timed_cb(timed_cb_direct_uploader_);
            timed_cb_direct_uploader_ = nullptr;
        }
        transport_->disconnect();
        transport_->stop();
    }

    if (direct_upload_timer_) {
        direct_upload_timer_->stop();
    }

    if (http_) {
        http_->disconnect();
        http_->stop();
    }

    _stop_all_streams(true);
}

/**
 * @brief on_prepared called by protocol handler when we are ready to send
 *        our own commands(events, status etc.)
 */
void manager::on_prepared() {
    if (callback_) {
        // Get memory card status from user, cache, send memorycard event
        // _update_storage_status();
    }
}

void manager::on_closed(int error, bye_reason reason) {
    logger->debug("on_closed: error {} reason {}", error, json(reason).dump());

    _stop_all_streams();
    _stop_all_event_streams();

    // Count accident disconnections
    if (reason == proto::BR_CONN_CLOSE)
        profile::global::instance().stats.cloud_reconnects++;

    // Notify userland
    callback_->on_bye(reason);

    // Try to reconnect in 5 or specified in the bye message from server seconds
    // Doesn't make sense to reconnect in case of auth failure.
    if (reason != BR_AUTH_FAILURE &&
        (reason == BR_RECONNECT || auto_reconnect_)) {
        int timeout = (reason == BR_RECONNECT) ? bye_retry_ * 1000 : 5000;
        if (!timeout)
            timeout = 5000;

        if (reason == BR_RECONNECT)
            logger->info("Reconnect scheduled in {} ms", timeout);
        else {
            logger->info("Connect scheduled in {} ms", timeout);
            // If the disconnection reason was not BR_RECONNECT request from the
            // Cloud we must connect to the main server to update reconnection
            // address
            cm_config_.reconnect_server_address.clear();
        }

        reconnect(timeout);
    }
}

bool manager::on_get_stream_config(proto::stream_config& config) {
    bool result = true;
    for (auto& s : streams_) {
        result |= s->get_stream_config(config);
    }
    return result;
}

bool manager::on_set_stream_config(const proto::stream_config& config) {
    bool result = true;

    for (auto& s : streams_) {
        proto::supported_stream_config es_streams;
        proto::stream_config current_stream_conf;
        bool apply = false;

        // Get ES streams provided by media::stream
        if (s->get_supported_stream(es_streams)) {
            // Get current video ES settings if stream has video
            if (!__is_unset(es_streams.video)) {
                proto::video_stream_config v;
                v.stream = es_streams.video;
                current_stream_conf.video.push_back(v);
            }

            // Get current audio ES settings if stream has audio
            if (!__is_unset(es_streams.audio)) {
                proto::audio_stream_config a;
                a.stream = es_streams.audio;
                current_stream_conf.audio.push_back(a);
            }

            s->get_stream_config(current_stream_conf);
        }

        // If current settings for specific ES is changed we need to apply them
        // and restart stream
        for (auto& v : config.video) {
            proto::video_stream_config vc;
            for (auto& c : current_stream_conf.video)
                if (c.stream == v.stream)
                    vc = c;
            if (es_streams.video == v.stream) {
                if (v != vc) {
                    logger->info(
                        "Settings for video stream {} changed, stream {} will "
                        "be restarted",
                        v.stream, s->cloud_name());
                    apply = true;
                } else {
                    logger->info(
                        "Settings for video stream {} NOT changed, stream {} "
                        "will NOT be restarted",
                        v.stream, s->cloud_name());
                }
            }
        }

        for (auto& a : config.audio) {
            proto::audio_stream_config ac;
            for (auto& c : current_stream_conf.audio)
                if (c.stream == a.stream)
                    ac = c;
            if (es_streams.audio == a.stream) {
                if (a != ac) {
                    logger->info(
                        "Settings for audio stream {} changed, stream {} will "
                        "be restarted",
                        a.stream, s->cloud_name());
                    apply = true;
                } else {
                    logger->info(
                        "Settings for audio stream {} NOT changed, stream {} "
                        "will NOT be restarted",
                        a.stream, s->cloud_name());
                }
            }
        }

        if (apply) {
            bool restart_by_event = false;
            result |= s->set_stream_config(config);

            if (result) {
                // VXG Cloud doesn't know if we stopped by_event recording so
                // we must start it by ourselves if the stream settings changed
                if (s == record_stream_ &&
                    (record_by_event_ || stream_by_event_))
                    restart_by_event = true;

                // Stop stream after the settings were configured to apply them
                // The cloud restarts disconnected stream
                _stop_stream(s);

                // Restart recording
                if (s == record_stream_) {
                    // Save permanent recording status, mark all recordings as
                    // false to stop recording, then we restore the saved
                    // permanent recording status and start everything again
                    auto permanent_recording_saved =
                        recording_status_[record_stream_].permanent;
                    recording_status_[record_stream_].by_event = false;
                    recording_status_[record_stream_].permanent = false;
                    _stop_recording(s);
                    recording_status_[record_stream_].permanent =
                        permanent_recording_saved;
                    _start_recording(s);
                }

                // Start record_by_event stream
                if (restart_by_event) {
                    transport_->run_on_rx_thread([=]() {
                        on_stream_start(s->cloud_name(), 0,
                                        proto::SR_RECORD_BY_EVENT);
                    });
                }
            }
        }
    }

    return result;
}

bool manager::on_get_motion_detection_config(
    proto::motion_detection_config& config) {
    return callback_->on_get_motion_detection_config(config);
}

bool manager::on_set_motion_detection_config(
    const proto::motion_detection_config& config) {
    return callback_->on_set_motion_detection_config(config);
}

bool manager::on_get_cam_video_config(proto::video_config& config) {
    return callback_->on_get_cam_video_config(config);
}

bool manager::on_set_cam_video_config(const proto::video_config& config) {
    return callback_->on_set_cam_video_config(config);
}

bool manager::_request_direct_upload_video(
    proto::get_direct_upload_url direct_upload) {
    if (auto s = lookup_stream(direct_upload.stream_id)) {
        cloud::time begin =
            utils::time::from_iso_packed(direct_upload.file_time);
        // Use microseconds duration if presented for for accurate export,
        // default duration in milliseconds floors the real value of duration.
        cloud::duration duration =
            (direct_upload.duration_us == cloud::duration(0))
                ? std::chrono::milliseconds(direct_upload.duration)
                : direct_upload.duration_us;
        cloud::time end = begin + duration;

        {
            auto export_start = std::chrono::system_clock::now();
            auto clip = s->record_export(begin, end);
            auto export_duration =
                std::chrono::system_clock::now() - export_start;
            logger->info(
                "Export took {:.3f}s",
                (double)std::chrono::duration_cast<std::chrono::milliseconds>(
                    export_duration)
                        .count() /
                    1000);
            if (!clip.data.empty() && clip.tp_start != utils::time::null() &&
                clip.tp_stop != utils::time::null()) {
                direct_upload.size = clip.data.size();
                records_[direct_upload.msgid] = std::move(clip);

                logger->info("Requesting upload url for {:.3f}Mb of {} id {}",
                             direct_upload.size / (double)(1024 * 1024),
                             json(direct_upload.category).dump(),
                             direct_upload.msgid);

                auto timeout = std::chrono::seconds(20);
                // Request upload URL, wait for reply for 20 seconds
                send_command_wait_ack(
                    json(direct_upload),
                    [=](bool timedout,
                        proto::command::base_command::ptr ack_cmd) {
                        // If no direct_upload_url was received from the Cloud
                        if (timedout) {
                            direct_upload.on_finished(false);
                            // Drop record
                            records_.erase(direct_upload.msgid);
                            logger->warn(
                                "No reply for record direct upload request "
                                "after "
                                "{} seconds, dropping record {}.",
                                timeout.count(), direct_upload.msgid);
                            profile::global::instance()
                                .stats.records_upload_failed++;
                        } else {
                            if (dynamic_pointer_cast<
                                    proto::command::direct_upload_url_base>(
                                    ack_cmd)
                                    ->status == "OK") {
                                dynamic_pointer_cast<
                                    proto::command::direct_upload_url_base>(
                                    ack_cmd)
                                    ->on_finished = direct_upload.on_finished;
                                dynamic_pointer_cast<
                                    proto::command::direct_upload_url_base>(
                                    ack_cmd)
                                    ->is_canceled = direct_upload.is_canceled;
                            } else {
                                logger->warn("Direct upload request error!");
                                direct_upload.on_finished(false);
                            }
                        }
                    },
                    // Wait for reply with direct upload url for several seconds
                    timeout);
                return true;
            } else {
                logger->warn("Unable to export record, skipping");
                profile::global::instance().stats.records_upload_failed++;
            }
        }
    }

    logger->warn("Record direct upload {} failed!", direct_upload.msgid);
    direct_upload.on_finished(false);

    return false;
}

bool manager::_request_direct_upload_snapshot(
    proto::get_direct_upload_url direct_upload) {
    logger->info("Requesting direct upload url for {:.3f}Mb of {}",
                 direct_upload.size / (double)(1024 * 1024),
                 json(direct_upload.category).dump());

    // Request upload URL
    send_command(json(direct_upload));
    return true;
}

bool manager::direct_upload_sync_cb() {
    static int counter = 0;
    if (counter++ % 10 == 0)
        logger->debug(
            "Direct upload scheduler awaken, current uploads {} pre uploads "
            "{} max uploads {} planned uploads {}",
            profile::global::instance().stats.records_uploading,
            records_.size(),
            profile::global::instance().max_concurent_video_uploads,
            direct_upload_video_q_.size());

    // Delete late uploads
    _update_direct_upload_queue_latency();

    profile::global::instance().stats.records_upload_queued =
        direct_upload_video_q_.size();

    if (registered_) {
        while (profile::global::instance().stats.records_uploading <
                   profile::global::instance().max_concurent_video_uploads &&
               direct_upload_video_q_.size() > 0 &&
               records_.size() <
                   profile::global::instance().max_concurent_video_uploads) {
            auto direct_upload_obj = direct_upload_video_q_.front();
            direct_upload_video_q_.pop_front();

            if (direct_upload_obj.is_canceled()) {
                logger->info("Planned direct upload id {} canceled.",
                             direct_upload_obj.msgid);
                continue;
            }

            if (direct_upload_obj.category == proto::UC_RECORD)
                if (_request_direct_upload_video(direct_upload_obj))
                    break;
        }
    }

    timed_cb_direct_uploader_ = direct_upload_timer_->schedule_timed_cb(
        [=] { direct_upload_sync_cb(); }, 1000);
    return true;
}

//! @brief Delete all planned direct upload requests older than allowed
void manager::_update_direct_upload_queue_latency() {
    direct_upload_video_q_.erase(
        std::remove_if(
            direct_upload_video_q_.begin(), direct_upload_video_q_.end(),
            [&](const proto::get_direct_upload_url& upload) {
                bool late = (utils::time::from_iso_packed(upload.file_time) <
                             (utils::time::now() -
                              profile::global::instance()
                                  .max_video_uploads_queue_lateness)) &&
                            upload.memorycard_sync_ticket.empty();
                if (late) {
                    logger->info("Drop too late planned direct upload id {}",
                                 upload.msgid);
                    upload.on_finished(false);
                    profile::global::instance().stats.records_upload_failed++;
                }
                return late;
            }),
        direct_upload_video_q_.end());
}

bool manager::_schedule_direct_upload(
    proto::get_direct_upload_url get_upload_url) {
    get_upload_url.cam_id = cm_config_.cam_id;

    if (get_upload_url.is_canceled()) {
        logger->info("Planned direct upload id {} canceled.",
                     get_upload_url.msgid);
        return false;
    }

    auto f = std::bind(
        [=](proto::get_direct_upload_url direct_upload_req) {
            logger->info("Enqueue {} id {} to direct upload queue",
                         direct_upload_req.category == proto::UC_RECORD
                             ? "record"
                             : "snapshot",
                         direct_upload_req.msgid);

            if (direct_upload_req.category == proto::UC_RECORD)
                direct_upload_video_q_.push_back(direct_upload_req);
            else if (direct_upload_req.category == proto::UC_SNAPSHOT)
                direct_upload_snapshot_q_.push_back(direct_upload_req);
            else
                logger->warn("Can't enqueue {} for direct upload",
                             json(direct_upload_req.category).dump());
        },
        get_upload_url);

    // FIXME:
    direct_upload_timer_->schedule_timed_cb(f, 1);
    return true;
}

bool manager::_cancel_direct_uploads_by_ticket(std::string ticket) {
    auto f = std::bind(
        [&](std::string ticket) {
            auto new_end = std::remove_if(
                direct_upload_video_q_.begin(), direct_upload_video_q_.end(),
                [&](const proto::get_direct_upload_url& upload) {
                    bool cancel = (!upload.memorycard_sync_ticket.empty() &&
                                   (ticket == upload.memorycard_sync_ticket));
                    if (cancel)
                        logger->info("Canceling planned record upload id {}",
                                     upload.msgid);
                    return cancel;
                });
            direct_upload_video_q_.erase(new_end, direct_upload_video_q_.end());
        },
        ticket);

    // FIXME:
    direct_upload_timer_->schedule_timed_cb(f, 1);
    return true;
}

bool manager::_handle_stream_stateful_event(
    proto::event_object& event,
    event_state::stream_delivery_mode delivery_mode) {
    using namespace std::chrono;

    if (event.active == events_states_[event.name()].active()) {
        logger->debug("Wrong {} state change {} -> {} !!!", event.name(),
                      events_states_[event.name()].active(), event.active);
        return false;
    } else if (!events_states_[event.name()].active() &&
               events_states_[event.name()].start() ==
                   utils::time::from_double(event.time)) {
        logger->debug(
            "New event {} with the same time {}, ignorring !!!", event.name(),
            utils::time::to_iso(utils::time::from_double(event.time)));
        return false;
    }

    logger->info("Event {} {} at {}", event.name(),
                 event.active ? "STARTED" : "FINISHED",
                 utils::time::to_iso(utils::time::from_double(event.time)));

    if (utils::time::from_double(event.time) == utils::time::null()) {
        logger->warn("Event {} time is wrong, skipping", event.name());
        return false;
    }

    if (event.name() == "motion")
        profile::global::instance().stats.motion_events++;

    if (event.active) {
        events_states_[event.name()].start(delivery_mode, event,
                                           pre_record_time_, post_record_time_,
                                           record_stream_);
    } else if (events_states_[event.name()].active()) {
        events_states_[event.name()].stop(utils::time::from_double(event.time));
    }

    return true;
}

bool manager::_handle_stream_stateless_event(
    proto::event_object& event,
    event_state::stream_delivery_mode delivery_mode) {
    using namespace std::chrono;

    logger->info("Event {} TRIGGERED at {}", event.name(),
                 utils::time::to_iso(utils::time::from_double(event.time)));

    if (utils::time::from_double(event.time) == utils::time::null()) {
        logger->warn("Event {} time is wrong, skipping", event.name());
        return false;
    }

    // This chunk was recorded by current record_stream_ or at least
    // current record_stream_ can export records
    // upload_token may be empty, used only if user's code specified it
    // for custom event alongside the event_object.upload_canceler flag.
    events_states_[event.name()].start(delivery_mode, event, pre_record_time_,
                                       post_record_time_, record_stream_,
                                       event.upload_token);
    return true;
}

bool manager::handle_event_snapshot(proto::event_object& event) {
    proto::event_config eventConfig;

    // We do not concurrently upload more snapshots than allowed
    // by the user for preventing OOM
    if (profile::global::instance().stats.snapshots_uploading >=
            profile::global::instance().max_concurent_snapshot_uploads ||
        snapshots_.size() >
            profile::global::instance().max_concurent_snapshot_uploads) {
        profile::global::instance().stats.snapshots_upload_failed++;
        logger->warn(
            "No space in snapshot upload queue, currently "
            "uploading {} in pre upload queue {} max allowed {}, "
            "generating event without snapshot!",
            profile::global::instance().stats.snapshots_uploading,
            snapshots_.size(),
            profile::global::instance().max_concurent_snapshot_uploads);
        return false;
    }

    /** if event supports snapshots and snapshot generation enabled we place
     *  @snapshot_info_object into the snapshots upload queue with CM
     * 'cam_event' command @id, SRV should answer with 'direct_upload_url'
     * command with snapshot upload url and snapshot id as a @refid id.
     *
     *  @see on_direct_upload_url
     */
    if (__is_unset(event.snapshot_info.size) ||
        event.snapshot_info.image_data.empty()) {
        logger->debug(
            "Event {} requires snapshot but no snapshot was attached!",
            event.name());

        event.snapshot_info.width =
            __is_unset(event.snapshot_info.width)
                ? profile::global::instance().default_snapshot_width
                : event.snapshot_info.width;
        event.snapshot_info.height =
            __is_unset(event.snapshot_info.height)
                ? profile::global::instance().default_snapshot_height
                : event.snapshot_info.height;

        if (snapshot_stream_) {
            if (!snapshot_stream_->get_snapshot(event.snapshot_info)) {
                logger->warn("Unable to get snapshot");
                profile::global::instance().stats.snapshots_capture_failed++;
            } else {
                // VXG Cloud doc states we have to use event trigger time as
                // an image time
                event.snapshot_info.image_time = utils::time::to_iso_packed(
                    utils::time::from_double(event.time));
                return true;
            }
        } else {
            profile::global::instance().stats.snapshots_capture_failed++;
            logger->warn("No streams to take snapshot");
        }
    }

    auto event_snapshot_time_diff_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            utils::time::from_iso_packed(event.snapshot_info.image_time) -
            utils::time::from_double(event.time));
    if (event_snapshot_time_diff_ms > std::chrono::seconds(1)) {
        logger->warn(
            "event and snapshot time is to different! |{} - {}| == {}ms",
            utils::time::to_iso(utils::time::from_double(event.time)),
            utils::time::to_iso(
                utils::time::from_iso_packed(event.snapshot_info.image_time)),
            event_snapshot_time_diff_ms.count());
    }

    return false;
}

bool manager::handle_event_meta_file(proto::event_object& event) {
    if (!__is_unset(event.meta_file)) {
        // We do not concurrently upload more meta files than allowed
        // by the user for preventing OOM and spamming http transport instance,
        // which uploads everything in a round-robin manier, too many meta files
        // may significantly slow down the overall upload speed
        if (profile::global::instance().stats.file_meta_uploading >=
                profile::global::instance().max_concurent_file_meta_uploads ||
            meta_files_.size() >
                profile::global::instance().max_concurent_file_meta_uploads) {
            profile::global::instance().stats.file_meta_upload_failed++;
            logger->trace(
                "No space in file meta upload queue, currently "
                "uploading {} in pre upload queue {} max allowed {}, "
                "generating event without file meta!",
                profile::global::instance().stats.file_meta_uploading,
                meta_files_.size(),
                profile::global::instance().max_concurent_file_meta_uploads);
            return false;
        }

        event.file_meta_info.size = event.meta_file.size();
        event.file_meta_info.data = event.meta_file;
        return true;
    }

    return false;
}

bool manager::__notify_record_event(std::string stream_id, bool on) {
    proto::command::cam_event event_command;

    if (!on && record_event_depth_ == 0)
        return false;

    record_event_depth_ += (on ? 1 : -1);
    logger->debug("record_event_depth_ {} on {}", record_event_depth_, on);
    if ((on && record_event_depth_ == 1) || (!on && record_event_depth_ == 0)) {
        event_command.event = proto::ET_RECORD;
        event_command.time = utils::time::to_double(utils::time::now());
        event_command.status = proto::ES_OK;

        // Inform Cloud about record should be started
        event_command.record_info.on = on;
        event_command.record_info.stream_id = stream_id;

        // Append camera id
        event_command.cam_id = cm_config_.cam_id;

        return send_command(json(event_command));
    }

    return false;
}

manager::event_state::stream_delivery_mode manager::_current_delivery_mode() {
    // Depending on record_by_event was started and memory card is presented
    // we choose video delivery mode
    event_state::stream_delivery_mode delivery_mode = event_state::SDM_NONE;
    if (record_by_event_ && record_stream_)
        // Upload recorded video
        delivery_mode = event_state::SDM_UPLOAD;
    else if (stream_by_event_ && record_stream_)
        // No memory card, start live streaming, no pre recorded video will
        // be delivered
        delivery_mode = event_state::SDM_STREAM;
    else
        // No need to send video
        delivery_mode = event_state::SDM_NONE;

    return delivery_mode;
}

/**
 * manager::notify_event() - Notify Server about new event
 *
 * @param event @proto::event_object argument
 * @return if event notification was successfull
 */
bool manager::notify_event(proto::event_object event) {
    using namespace std::chrono;
    bool need_snapshot = false;
    bool need_meta = false;
    proto::event_config event_config;
    proto::command::cam_event::ptr event_command =
        dynamic_pointer_cast<cam_event>(factory::create(CAM_EVENT));

    if (!registered_) {
        logger->warn(
            "Unable to notify the event until we aren't connected to the "
            "server!");
        return false;
    }

    /** check if event was configured in on_get_cam_events_config() callback
     *  unknown events are ignored
     */
    if (!events_config_.get_event_config(event, event_config)) {
        logger->warn(
            "Unable to find config for event {}, did you specify "
            "agent::callback::on_get_cam_events_config() or "
            "agent::event_stream::get_events()?",
            event.name());
        return false;
    }

    if (event.state_dummy)
        logger->debug("Dummy event {}", event.name());

    // Check if event was disabled
    if (!event_config.active && !event.state_dummy) {
        logger->warn("Notified event {} disabled by the Cloud API",
                     event.name());
        return false;
    }

    /* check event info_ struct if presented/required */
    switch (event.event) {
        case proto::ET_MOTION:
            if (profile::global::instance().attach_qos_report_to_motion) {
                event.meta = profile::global::instance().stats;
            }
            break;
        case proto::ET_CUSTOM:
            break;
        case proto::ET_WIFI: {
            proto::event_object::wifi_info_object wifi_info;

            if (event.wifi_info.state == proto::WNS_INVALID) {
                logger->warn("wifi event's wifi_info.state is not set!");
                return false;
            }

            if (__is_unset(event.wifi_info.ssid)) {
                logger->warn("wifi event's wifi_info.ssid is not set!");
                return false;
            }

            if (std::find(event.wifi_info.encryption_caps.begin(),
                          event.wifi_info.encryption_caps.end(),
                          event.wifi_info.encryption) ==
                event.wifi_info.encryption_caps.end()) {
                logger->warn(
                    "wifi event's wifi_info.encryption should be set with the "
                    "value presented in wifi_info.encryption_caps list!");
                return false;
            }
        } break;
        case proto::ET_MEMORYCARD: {
            if (event.memorycard_info.status == proto::MCS_INVALID) {
                if (callback_->on_get_memorycard_info(memorycard_info_)) {
                    event.memorycard_info = memorycard_info_;
                } else
                    logger->warn("Unable to get memorycard info");
            }

            // If recording was requested by the Cloud but not started yet we
            // are trying to start it.
            // Or if recording was started but accidentally stopped not by us
            // we are trying to start it.
            if (record_stream_ &&
                event.memorycard_info.status == proto::MCS_NORMAL &&
                event.memorycard_info.recording_status !=
                    alter_bool::B_INVALID) {
                bool force_stop = false;
                if (recording_status_[record_stream_].started &&
                    !event.memorycard_info.recording_status) {
                    // We need to stop recording inside of the manager to keep
                    // recording state tracking
                    recording_status_[record_stream_].external_stop_count++;
                    force_stop = true;
                    logger->warn(
                        "Recording was stopped from outside {} times already!",
                        recording_status_[record_stream_].external_stop_count);
                }

                recording_status_[record_stream_].started =
                    event.memorycard_info.recording_status;

                logger->info(
                    "Memorycard event notified, recording is {}",
                    recording_status_[record_stream_].started ? "on" : "off");
                if (event.memorycard_info.status == proto::MCS_NORMAL) {
                    if (force_stop)
                        _stop_recording(record_stream_, true);
                    _start_recording(record_stream_);
                }
            }
        } break;
        case proto::ET_SOUND: {
        } break;
        default:
            logger->info("NYI event {} notified", event.name());
            break;
    }

    if (event.upload_canceler && !event.upload_token.empty()) {
        logger->info("Event {} is upload canceler for uploads with token '{}'",
                     event.name(), event.upload_token);
        uploader_->cancel_upload(event.upload_token);
        _cancel_direct_uploads_by_ticket(event.upload_token);
    }

    // Handle records uploading
    if (event_config.caps.stream && event_config.stream && !event.state_dummy) {
        auto delivery_mode = _current_delivery_mode();

        if (event_config.caps.statefull) {
            if (!_handle_stream_stateful_event(event, delivery_mode))
                return false;

            // Nothing to do on statefull event stop
            if (!event.active)
                return true;
        } else
            _handle_stream_stateless_event(event, delivery_mode);
    }

    // Event requires snapshot if it was configured correspondingly
    // Send snapshot for stateless events and for start of stateful event
    // Send snapshot if stateful_event_continuation_kick_snapshot is true and
    // this is a dummy event for event state emulation
    need_snapshot = event_config.snapshot &&
                    (((event_config.caps.statefull && event.active) ||
                      !event_config.caps.statefull) ||
                     (profile::global::instance()
                          .stateful_event_continuation_kick_snapshot &&
                      event.state_dummy));

    if (need_snapshot && (need_snapshot = handle_event_snapshot(event))) {
        snapshots_[event_command->msgid] = event.snapshot_info;

        logger->debug("Event snapshot {} enqueued to upload queue with id {}",
                      event.snapshot_info.image_time.c_str(),
                      event_command->msgid);
    }

    if (!event.state_dummy && (need_meta = handle_event_meta_file(event))) {
        meta_files_[event_command->msgid] = event.file_meta_info;

        logger->debug(
            "Event meta file of {} bytes enqueued to upload queue with id {}",
            event.file_meta_info.size, event_command->msgid);
    }

    if (event_config.caps.internal_hidden) {
        logger->debug("Not sending reported internal event '{}' to the Cloud",
                      event.name());
        return false;
    }

    event.status = proto::ES_OK;

    *static_cast<proto::event_object*>(event_command.get()) = event;
    // Append camera id
    event_command->cam_id = cm_config_.cam_id;

    // If we specified snapshot or meta_file info struct in the cam_event
    // the server should reply with direct_upload_url command
    if (need_snapshot || need_meta) {
        // Send cam_event with snapshot info, Cloud should reply with direct
        // upload url, we wait for this reply for several seconds and then
        // drop the snapshot
        return send_command_wait_ack(
            json(*event_command),
            [=](bool timedout, proto::command::base_command::ptr not_used) {
                if (timedout) {
                    if (need_snapshot)
                        snapshots_.erase(event_command->msgid);
                    if (need_meta)
                        meta_files_.erase(event_command->msgid);

                    logger->warn(
                        "No reply for cam_event id {} with extra "
                        "payload[snapshot: {}, file_meta: {}]",
                        event_command->msgid, need_snapshot, need_meta);
                    profile::global::instance().stats.snapshots_upload_failed +=
                        need_snapshot;
                    profile::global::instance().stats.file_meta_upload_failed +=
                        need_meta;
                }
                // Otherwise we were notified about direct_upload_url
            },
            std::chrono::seconds(20));
    }

    return send_command(json(*event_command));
}

bool manager::on_direct_upload_url(const direct_upload_url_base& direct_upload,
                                   int event_id,
                                   int refid) {
    std::vector<uint8_t> data;
    std::string media_type;
    bool has_category = (direct_upload.category != UC_INVALID);

    if (snapshots_.count(refid) &&
        ((has_category && direct_upload.category == UC_SNAPSHOT) ||
         (!has_category))) {
        logger->info("Uploading snapshot {}", snapshots_[refid].image_time);

        // No copy here, we move resources
        data = std::move(snapshots_[refid].image_data);
        media_type = "Snapshot";

        if (!data.empty())
            profile::global::instance().stats.snapshots_uploading++;

        snapshots_.erase(refid);
    } else if (records_.count(refid) &&
               ((has_category && direct_upload.category == UC_RECORD) ||
                (!has_category))) {
        logger->info("Uploading video chunk id {} size {} time {} -- {}", refid,
                     records_[refid].data.size(),
                     utils::time::to_iso(records_[refid].tp_start),
                     utils::time::to_iso(records_[refid].tp_stop));
        // No copy here, we move resources
        data = std::move(records_[refid].data);
        media_type = "Record";

        if (!data.empty() && !direct_upload.is_canceled())
            profile::global::instance().stats.records_uploading++;

        // TODO:
        records_.erase(refid);
    } else if (meta_files_.count(refid) &&
               ((has_category && direct_upload.category == UC_FILE_META) ||
                (!has_category))) {
        if (!meta_files_[refid].data.empty()) {
            logger->info("Uploading meta file id {} size {}", refid,
                         meta_files_[refid].size);
            data.insert(data.end(), meta_files_[refid].data.begin(),
                        meta_files_[refid].data.end());
        }
        media_type = "Meta";

        if (!data.empty())
            profile::global::instance().stats.file_meta_uploading++;

        // TODO:
        meta_files_.erase(refid);
    }

    if (!data.empty() && !direct_upload.is_canceled()) {
        auto req = std::make_shared<request>(direct_upload.url, "PUT", data);

        for (auto it = direct_upload.headers.begin();
             it != direct_upload.headers.end(); it++) {
            if (it->first != "Content-Length")
                req->set_header(it->first, it->second);
        }

        // NOTE: Don't use req variable inside the lambda!
        // weak_ptr required, if we capture shared_ptr what the request_ptr is,
        // we will cycle the reference to shared_ptr and cause the memory leak
        std::weak_ptr<request> weak_req(req);
        req->set_response_cb([=](std::string& resp, int code) {
            logger->info("HTTP request finished with code {}", code);

            direct_upload.on_finished(code == 200);

            if (media_type == "Snapshot") {
                profile::global::instance().stats.snapshots_uploading--;
            } else if (media_type == "Record") {
                profile::global::instance().stats.records_uploading--;
            } else if (media_type == "Meta") {
                profile::global::instance().stats.file_meta_uploading--;
            }

            if (code == 200) {
                if (media_type == "Snapshot") {
                    profile::global::instance().stats.snapshots_uploaded++;
                } else if (media_type == "Record") {
                    profile::global::instance().stats.records_uploaded++;
                } else if (media_type == "Meta") {
                    profile::global::instance().stats.file_meta_uploaded++;
                }

                if (auto r = weak_req.lock()) {
                    logger->info(
                        "{} with id {} successfully uploaded in {:.3f} "
                        "seconds, "
                        "speed {:.3f} KB/s",
                        media_type, refid,
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            r->time_stop - r->time_start)
                                .count() /
                            (double)1000,
                        r->upload_speed / (double)1024);

                    if (media_type == "Snapshot") {
                        profile::global::instance()
                            .stats.snapshot_last_upload_speed_KBps =
                            std::round((r->upload_speed / (double)1024) *
                                       1000) /
                            1000;
                    } else if (media_type == "Record") {
                        profile::global::instance()
                            .stats.records_last_upload_speed_KBps =
                            std::round((r->upload_speed / (double)1024) *
                                       1000) /
                            1000;
                    }
                }
            } else {
                if (auto r = weak_req.lock())
                    logger->error(
                        "{} {} upload failed with code {} after {:.3f} seconds",
                        media_type, refid, code,
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            r->time_stop - r->time_start)
                                .count() /
                            (double)1000);
                else
                    logger->error("{} {} upload failed with code {}",
                                  media_type, refid, code);

                logger->error("Response: {}", resp);

                if (media_type == "Snapshot")
                    profile::global::instance().stats.snapshots_upload_failed++;
                else if (media_type == "Record")
                    profile::global::instance().stats.records_upload_failed++;
                else if (media_type == "Meta")
                    profile::global::instance().stats.file_meta_upload_failed++;
            }
        });

        req->max_upload_speed = profile::global::instance().max_upload_speed;
        return http_->make(req);
    } else {
        if (direct_upload.is_canceled())
            logger->info("Direct upload {} has been canceled.", refid);
        else
            logger->error("No snapshot, record or meta for refid {}", refid);
        // Notify upload initiator we are failed
        direct_upload.on_finished(false);
    }

    return false;
}

void manager::__trigger_periodic_event(const proto::event_config& event_conf) {
    using namespace std::chrono;
    proto::event_object periodic_event;
    periodic_event.event = event_conf.event;
    periodic_event.custom_event_name = event_conf.custom_event_name;
    periodic_event.time = static_cast<double>(utils::time::now_time_UTC());
    periodic_event.status = proto::ES_OK;

    if (event_conf.custom_event_name == "qos-report")
        periodic_event.meta = json(profile::global::instance().stats);

    notify_event(periodic_event);

    if (event_conf.caps.periodic) {
        periodic_events_[event_conf.name()] = transport_->schedule_timed_cb(
            std::bind(&manager::__trigger_periodic_event, this, event_conf),
            event_conf.period * 1000);
    }
}

void manager::_schedule_periodic_event(const proto::event_config& event_conf) {
    if (event_conf.caps.periodic && event_conf.active) {
        size_t period = event_conf.period;

        if (period == 0) {
            logger->warn(
                "User tried to set period to 0 via API, treating it as "
                "disabled event!");
            return;
        }

        auto p = transport_->schedule_timed_cb(
            std::bind(&manager::__trigger_periodic_event, this, event_conf),
            period * 1000);
        logger->info("Starting periodic event {} period {} timer {}",
                     event_conf.name(), period, (void*)p.get());
        if (p) {
            if (periodic_events_.count(event_conf.name()) &&
                periodic_events_[event_conf.name()])
                transport_->cancel_timed_cb(
                    periodic_events_[event_conf.name()]);
            periodic_events_[event_conf.name()] = p;
        } else
            logger->warn("Failed to start periodic event");
    }
}

void manager::_cancel_periodic_event(const proto::event_config& event_conf) {
    if (periodic_events_.count(event_conf.name())) {
        logger->info("Stopping periodic event {} timer {}", event_conf.name(),
                     (void*)periodic_events_[event_conf.name()].get());

        if (periodic_events_[event_conf.name()]) {
            transport_->cancel_timed_cb(periodic_events_[event_conf.name()]);
            periodic_events_[event_conf.name()] = nullptr;
        }
        periodic_events_.erase(event_conf.name());
    }
}

void manager::_schedule_periodic_events(proto::events_config& events) {
    for (auto& event_conf : events.events) {
        if (event_conf.caps.periodic)
            _schedule_periodic_event(event_conf);
    }
}

void manager::_cancel_periodic_events(const proto::events_config& events) {
    for (auto& event_conf : events.events) {
        _cancel_periodic_event(event_conf);
    }
}

void manager::_append_internal_custom_events(proto::events_config& config) {
    if (profile::global::instance().send_qos_report_as_separate_event) {
        logger->info("Append QOS event to events config");

        std::vector<proto::event_config> conf_list;
        proto::event_config qos_event_config;

        qos_event_config.event = proto::ET_CUSTOM;
        qos_event_config.custom_event_name = "qos-report";
        qos_event_config.active = true;
        qos_event_config.period =
            profile::global::instance().send_qos_report_period_sec;
        qos_event_config.snapshot = false;
        qos_event_config.stream = false;

        qos_event_config.caps.periodic = true;
        qos_event_config.caps.trigger = false;
        qos_event_config.caps.stream = false;
        qos_event_config.caps.snapshot = false;

        conf_list.push_back(qos_event_config);

        // Append if not yet in config, update otherwise
        if (std::find_if(config.events.begin(), config.events.end(),
                         [=](const proto::event_config& c) {
                             return qos_event_config.name_eq(c);
                         }) == config.events.end()) {
            config.events.push_back(qos_event_config);
        } else
            _update_events_configs(conf_list, config.events);
    }
}

void manager::_init_events_states(const proto::events_config& config) {
    events_states_.clear();
    for (auto& event_conf : config.events) {
        auto stream_start_cb = [=](std::string stream_id,
                                   proto::stream_reason r) {
            transport_->run_on_rx_thread(
                [=]() { on_stream_start(stream_id, -1, r); });
        };
        auto stream_stop_cb = [=](std::string stream_id,
                                  proto::stream_reason r) {
            transport_->run_on_rx_thread(
                [=]() { on_stream_stop(stream_id, r); });
        };

        events_states_[event_conf.name()] = event_state(
            event_conf, http_,
            std::bind(&manager::notify_event, this, placeholders::_1),
            uploader_, stream_start_cb, stream_stop_cb);
    }

    // Hidden from Cloud events, used inside the library only
    {
        // Not real event, used as timeline synchronization state storage
        proto::event_config event_conf;
        event_conf.caps.stream = true;
        event_conf.caps.internal_hidden = true;
        events_states_["timeline-sync"] = event_state(
            event_conf, http_,
            std::bind(&manager::notify_event, this, placeholders::_1),
            uploader_);
    }

    {
        // Not real event, used as direct upload record state storage
        // see 4.2 stream_start (SRV)[storage_direct_recording]
        proto::event_config event_conf;
        event_conf.caps.stream = true;
        event_conf.caps.statefull = true;
        event_conf.caps.internal_hidden = true;
        events_states_["direct-upload-recording"] = event_state(
            event_conf, http_,
            std::bind(&manager::notify_event, this, placeholders::_1),
            uploader_);
    }
}

bool manager::_update_events_configs(
    const std::vector<proto::event_config>& new_configs,
    std::vector<proto::event_config>& dest_configs) {
    for (auto& new_conf : new_configs) {
        auto it = std::find_if(dest_configs.begin(), dest_configs.end(),
                               [new_conf](const proto::event_config& c) {
                                   return new_conf.name_eq(c);
                               });

        if (it != dest_configs.end()) {
            it->active = new_conf.active;

            // Can apply settings only, caps may not be changed
            if (new_conf.caps_eq(*it)) {
                // Apply settings only, don't touch caps
                // Apply new flags values with respect to caps
                it->period = new_conf.period;
                it->snapshot = new_conf.snapshot && it->snapshot;
                it->stream = new_conf.stream && it->stream;
            } else {
                logger->error(
                    "Unable to apply config for event {}, capabilities "
                    "changed.",
                    json(new_conf)["event"]);
                return false;
            }
        } else {
            // No such event config yet, adding it
            dest_configs.push_back(new_conf);
        }
    }

    return true;
}

event_stream::ptr manager::_lookup_event_stream(const std::string& name) {
    auto es = std::find_if(event_streams_.begin(), event_streams_.end(),
                           [name](const event_stream::ptr event_stream) {
                               return name == event_stream->name();
                           });

    if (es != event_streams_.end())
        return *es;

    return nullptr;
}

bool manager::_update_event_stream_configs(
    const std::string& stream_name,
    const std::vector<proto::event_config>& new_configs) {
    std::vector<proto::event_config> temp_configs;

    logger->debug("event_streams_configs_ size {}, has {} keys {}",
                  event_streams_configs_.size(),
                  event_streams_configs_.count(stream_name), stream_name);

    // Initialization, caps should never be changed
    if (!event_streams_configs_.count(stream_name)) {
        event_streams_configs_[stream_name] = new_configs;
        return true;
    }

    return _update_events_configs(new_configs,
                                  event_streams_configs_[stream_name]);
}

event_stream::ptr manager::_lookup_event_stream_by_event(
    const proto::event_object& event) {
    for (auto& kv : event_streams_configs_) {
        auto e = std::find_if(kv.second.begin(), kv.second.end(),
                              [event](proto::event_config& ec) {
                                  return event.event == ec.event &&
                                         event.custom_event_name ==
                                             ec.custom_event_name;
                              });

        if (e != kv.second.end())
            return _lookup_event_stream(kv.first);
    }

    return nullptr;
}

void manager::_load_events_configs(proto::events_config& config) {
    logger->info("Internal events config initialization");

    // Callback events config, mostly used for custom events
    if (!callback_->on_get_cam_events_config(config)) {
        logger->warn("Unable to get events config via common callback");
        config = proto::events_config();
    }

    _append_internal_custom_events(config);

    // Per event stream configs, retreive events provided by all event streams
    for (auto& event_stream : event_streams_) {
        std::vector<proto::event_config> stream_events;

        // Retrieve events conf from event stream
        if (event_stream->get_events(stream_events)) {
            std::string events_str;
            for (std::vector<proto::event_config>::iterator ec =
                     stream_events.begin();
                 ec != stream_events.end(); ec++) {
                json jec = *ec;
                events_str += jec["event"];

                if (std::distance(ec, stream_events.end()) != 1)
                    events_str += ", ";
            }
            logger->info("Event stream {} provides {} events: {}",
                         event_stream->name(), stream_events.size(),
                         events_str);

            // Apply retrieved configs to event stream config holder and all
            // events configs holder
            if (!(_update_event_stream_configs(event_stream->name(),
                                               stream_events) &&
                  _update_events_configs(stream_events, config.events)))
                logger->error(
                    "Unable to apply events config for event stream {}",
                    event_stream->name());
        } else {
            logger->warn(
                "Event stream {} failed to report its events config, all "
                "events notified by this event stream will be ignored!",
                event_stream->name());
        }
    }

    if (events_states_.empty())
        _init_events_states(config);
}

bool manager::on_get_cam_events_config(proto::events_config& config) {
    bool result = true;

    // Stop all events providers
    _cancel_periodic_events(events_config_);
    _stop_all_event_streams();

    // Load events configs and caps before applying new settings
    if (events_config_.events.empty())
        _load_events_configs(events_config_);

    config = events_config_;
    // Don't report internal events
    config.events.erase(
        std::remove_if(
            config.events.begin(), config.events.end(),
            [](proto::event_config& ec) { return ec.caps.internal_hidden; }),
        config.events.end());

    // Start sending events only after get_events command will be sent to the
    // Cloud. This is ugly since we schedule it to run after one second but
    // there is no other way to postpone it rn.
    transport_->schedule_timed_cb(
        [=]() {
            // Start events providers
            // Start periodic events internal timers
            _schedule_periodic_events(events_config_);

            // Start event streams if required, event providers must immediately
            // notify states of all statefull events
            if (events_config_.enabled) {
                for (auto& event_stream : event_streams_) {
                    logger->info("Start event stream {}", event_stream->name());
                    event_stream->set_notification_cb(std::bind(
                        &manager::notify_event, this, placeholders::_1));
                    event_stream->start();
                }
            }
            // TODO: move memorycard event to internal events
            _update_storage_status();
        },
        1000);

    return result;
}

bool manager::on_set_cam_events_config(const proto::events_config& config) {
    // Disable all periodic events to apply new settings
    _cancel_periodic_events(events_config_);
    _stop_all_event_streams();

    // Load events configs and caps before applying new settings
    if (events_config_.events.empty())
        _load_events_configs(events_config_);

    // Update per-event stream configs, only already presented events for the
    // event stream will be updated
    for (auto& event_stream : event_streams_) {
        _update_event_stream_configs(event_stream->name(), config.events);
        event_stream->set_events(event_streams_configs_[event_stream->name()]);
    }

    // Save enabled flag
    events_config_.enabled = config.enabled;
    // Apply settings only, don't touch caps
    _update_events_configs(config.events, events_config_.events);
    // TODO: move memorycard event to internal events
    _update_storage_status();

    if (events_states_.empty())
        _init_events_states(events_config_);

    // Start/stop event streams according to global 'enabled' flag
    if (config.enabled) {
        for (auto& event_stream : event_streams_) {
            logger->info("Start event stream {}", event_stream->name());
            event_stream->set_notification_cb(
                std::bind(&manager::notify_event, this, placeholders::_1));
            event_stream->start();
        }
    }

    // Reschedule periodic events with new settings
    _schedule_periodic_events(events_config_);

    return callback_->on_set_cam_events_config(events_config_);
}

bool manager::on_get_cam_audio_config(proto::audio_config& config) {
    return callback_->on_get_cam_audio_config(config);
}

bool manager::on_set_cam_audio_config(const proto::audio_config& config) {
    return callback_->on_set_cam_audio_config(config);
}

bool manager::on_get_ptz_config(proto::ptz_config& config) {
    return callback_->on_get_ptz_config(config);
}

bool manager::on_cam_ptz(proto::ptz_command command) {
    return callback_->on_cam_ptz(command);
}

bool manager::on_cam_ptz_preset(proto::ptz_preset& preset_op) {
    return callback_->on_cam_ptz_preset(preset_op);
}

bool manager::on_get_osd_config(proto::osd_config& config) {
    return callback_->on_get_osd_config(config);
}

bool manager::on_set_osd_config(const proto::osd_config& command) {
    return callback_->on_set_osd_config(command);
}

bool manager::on_get_wifi_config(proto::wifi_config& config) {
    return callback_->on_get_wifi_config(config);
}

bool manager::on_set_wifi_config(const proto::wifi_network& config) {
    return callback_->on_set_wifi_config(config);
}

stream::ptr manager::lookup_stream(std::string name) {
    for (auto& s : streams_) {
        if (s->cloud_name() == name)
            return s;
    }

    return nullptr;
}

void manager::_stop_all_streams(bool sync) {
    if (record_stream_) {
        recording_status_[record_stream_].by_event = false;
        // Recording stop will not happen if permanent recording is enabled
        _stop_recording(record_stream_);
    }

    for (auto& s : streams_) {
        _stop_stream(s, sync);
    }
}

void manager::_stop_all_event_streams() {
    for (auto& s : event_streams_) {
        s->stop();
    }

    // Loop over all event states and stop all active statefull events
    auto now = utils::time::now();
    for (auto& state : events_states_) {
        if (state.second.stateful() && state.second.active() &&
            state.second.need_record()) {
            logger->info("Force stopping event {} at {}", state.first,
                         utils::time::to_iso(now));
            state.second.stop(now);
        }
    }
}

void manager::_stop_stream(stream::ptr s, bool sync) {
    if (!sync) {
        // Stream stop should always be serialized with transport rx thread.
        // We have to stop/start stream in the rx thread only.
        transport_->run_on_rx_thread([=]() {
            on_stream_stop(s->cloud_name(), proto::stream_reason::SR_LIVE);
            on_stream_stop(s->cloud_name(), proto::stream_reason::SR_RECORD);
            on_stream_stop(s->cloud_name(),
                           proto::stream_reason::SR_RECORD_BY_EVENT);
            on_stream_stop(s->cloud_name(),
                           proto::stream_reason::SR_SERVER_BY_EVENT);
        });
    } else {
        on_stream_stop(s->cloud_name(), proto::stream_reason::SR_LIVE);
        on_stream_stop(s->cloud_name(), proto::stream_reason::SR_RECORD);
        on_stream_stop(s->cloud_name(),
                       proto::stream_reason::SR_RECORD_BY_EVENT);
        on_stream_stop(s->cloud_name(),
                       proto::stream_reason::SR_SERVER_BY_EVENT);
    }
}

bool manager::_update_storage_status() {
    bool result = false;

    logger->debug("Update storage status");

    if (callback_->on_get_memorycard_info(memorycard_info_)) {
        proto::event_object event;
        event.event = proto::ET_MEMORYCARD;
        event.time = static_cast<double>(utils::time::now_time_UTC());
        event.memorycard_info = memorycard_info_;

        if (record_stream_) {
            if (memorycard_info_.recording_status != alter_bool::B_INVALID) {
                recording_status_[record_stream_].started =
                    memorycard_info_.recording_status;
            } else {
                logger->warn("Recording status is unknown!");
            }
        }

        notify_event(event);

        result = (memorycard_info_.status == proto::MCS_NORMAL);
    }

    return result;
}

bool manager::on_stream_start(const std::string& streamId,
                              int publishSessionID,
                              proto::stream_reason reason) {
    std::string url = cm_config_.buildUrl(
        manager_config::UT_STREAM_LIVE, streamId,
        publishSessionID >= 0 ? std::to_string(publishSessionID) : "");
    stream::ptr stream = lookup_stream(streamId);
    bool result = false;
    // Get memory card status from user
    bool storage_ok = false;

    if (!stream) {
        logger->error("Unable to find stream {}", streamId);
        return false;
    }

    logger->info("Start stream {}, reason {}, publish url {}",
                 stream->cloud_name(), json(reason).dump(), url.c_str());

    switch (reason) {
        case proto::stream_reason::SR_LIVE:
            logger->debug("Start LIVE");

            // If source already was started this function returns with true
            if (!stream->start()) {
                logger->error("Unable to start source");
                return false;
            }

            // Start only if no sink was already started
            if (!active_sinks_) {
                result = stream->start_sink(url);
                active_sinks_ += result;
            } else
                result = true;

            if (result)
                live_stream_ = stream;
            break;
        case proto::stream_reason::SR_SERVER_BY_EVENT:
            if (event_streams_.empty()) {
                logger->warn(
                    "Cloud starts event driven recording but no event streams "
                    "were provided by user!");
                return false;
            }

            logger->debug("Start SERVER_BY_EVENT, falling through");
            // Fall-through as for agent it's the same modes
        case proto::stream_reason::SR_RECORD:
            logger->debug("Start RECORD");

            // If source already was started this function returns with true
            if (!stream->start()) {
                logger->error("Unable to start source");
                return false;
            }

            // If by_event without memory card mode used we need to notify
            // Cloud with special record event to start the recording of the
            // live stream.
            if (stream_by_event_)
                __notify_record_event(stream->cloud_name(), true);

            // Start only if no sink was already started
            if (!active_sinks_) {
                result = stream->start_sink(url);
                active_sinks_ += result;
            } else
                result = true;

            if (result)
                record_stream_ = stream;
            break;
        case proto::stream_reason::SR_RECORD_BY_EVENT: {
            logger->debug("Start RECORD BY EVENT");

            if (event_streams_.empty()) {
                logger->warn(
                    "Cloud starts event driven recording but no event streams "
                    "were provided by user!");
                return false;
            }

            storage_ok = _update_storage_status();
            recording_status_[stream].by_event = true;
            recording_status_[stream].stream_id = streamId;

            if (storage_ok) {
                result = true;

                if (stream->record_needs_source()) {
                    // If source already was started this function returns true
                    if (!stream->start()) {
                        logger->error("Unable to start source");
                        return false;
                    }
                }

                // Event streams
                // TODO: should we notify all event streams here?
                for (auto& es : event_streams_) {
                    es->set_trigger_recording(true, pre_record_time_.count(),
                                              post_record_time_.count());
                }

                // Start recording, if we failed the recording_status_ will be
                // holding or wish to start in future
                _start_recording(stream);
                record_stream_ = stream;
                record_by_event_ = true;
            } else {
                logger->warn(
                    "Storage is not ready for recording, stream by event mode "
                    "will be used.");
                result = true;
                stream_by_event_ = true;
                record_stream_ = stream;
            }

            // Loop over all event states and switch delivery mode for statefull
            // events that were emitted before the by_event mode started
            for (auto& state : events_states_) {
                if (state.second.stateful() && state.second.active() &&
                    state.second.need_record()) {
                    logger->info("Switching delivery mode for event {} to {}",
                                 state.first, _current_delivery_mode());
                    state.second.update_delivery_mode(_current_delivery_mode(),
                                                      record_stream_);
                }
            }
        } break;
        case proto::stream_reason::SR_RECORD_DIRECT: {
            logger->debug("Start RECORD DIRECT UPLOAD");

            if (_update_storage_status()) {
                // Start recording, if we failed the recording_status_ will be
                // holding or wish to start in future
                continuos_direct_record_ = true;
                recording_status_[stream].direct_upload = true;
                record_stream_ = stream;
                _start_recording(stream);

                {
                    proto::event_object direct_upload_recording;
                    direct_upload_recording.event = proto::ET_CUSTOM;
                    direct_upload_recording.custom_event_name =
                        "direct-upload-recording";
                    direct_upload_recording.time =
                        utils::time::to_double(utils::time::now());

                    events_states_[direct_upload_recording.name()].start(
                        event_state::SDM_UPLOAD, direct_upload_recording,
                        std::chrono::seconds(0), std::chrono::seconds(0),
                        record_stream_, direct_upload_recording.upload_token);
                }

                result = true;
            } else {
                logger->debug(
                    "No memorycard to start direct upload recording, starting "
                    "normal continuous recording via live");
                result = on_stream_start(streamId, publishSessionID,
                                         proto::stream_reason::SR_RECORD);
            }
        } break;
        default:
            logger->error("Start INVALID");
            result = false;
    }

    logger->info("Stream {} start {} reason {} active sinks {}",
                 stream->cloud_name(), result ? "OK" : "Failed",
                 json(reason).dump(), active_sinks_);

    return result;
}

bool manager::on_stream_stop(const std::string& streamId,
                             proto::stream_reason reason) {
    stream::ptr stream = lookup_stream(streamId);

    if (!stream) {
        logger->error("Unable to find stream {}", streamId);
        return false;
    }

    logger->info("Stop stream {} reason {}", stream->cloud_name(),
                 json(reason).dump().c_str());

    switch (reason) {
        case proto::stream_reason::SR_LIVE:
            logger->debug("Stop LIVE");
            if (!active_sinks_)
                break;

            if (!--active_sinks_)
                stream->stop_sink();
            break;
        case proto::stream_reason::SR_RECORD:
            // SR_RECORD was started with storage_direct_recording flag
            if (continuos_direct_record_) {
                logger->debug("Stop RECORD DIRECT");
                events_states_["direct-upload-recording"].stop(
                    utils::time::now());
                continuos_direct_record_ = false;
                recording_status_[stream].direct_upload = false;
                _stop_recording(stream);
                break;
            }
        case proto::stream_reason::SR_SERVER_BY_EVENT:
            logger->debug("Stop RECORD");
            // If by_event without memory card mode used we need to notify
            // Cloud with special record event to stop the recording.
            if (stream_by_event_)
                __notify_record_event(stream->cloud_name(), false);

            if (!active_sinks_)
                break;

            if (!--active_sinks_) {
                stream->stop_sink();
            }
            break;
        case proto::stream_reason::SR_RECORD_BY_EVENT:
            logger->debug("Stop RECORD BY EVENT");
            if (!record_by_event_ && !stream_by_event_)
                break;

            if (record_stream_ == stream) {
                // TODO: SDM_UPLOAD should be properly stopped
                // TODO: This SDM_STREAM stop varian will cause stop of the live
                //       or record stream due to update_delivery will cause
                //       stream_stop with reason SR_RECORD after
                //       post_record_time_.
                //       This is not critical because cloud will restart stream
                //       anyway.
                if (stream_by_event_) {
                    // Loop over all event states and switch delivery mode for
                    // statefull events that were emitted before the by_event
                    // mode started
                    for (auto& state : events_states_) {
                        if (state.second.stateful() &&
                            (state.second.active() ||
                             state.second.post_stop_pending()) &&
                            state.second.need_record()) {
                            logger->info(
                                "Switching delivery mode for event {} to NONE",
                                state.first);
                            // Cancel post record timer stop, stop here instead
                            state.second.update_delivery_mode(
                                event_state::SDM_NONE, record_stream_);
                            on_stream_stop(stream->cloud_name(),
                                           proto::SR_RECORD);
                        }
                    }
                }

                // Send 'record' event before changing stream_by_event to false
                if (record_event_depth_) {
                    record_event_depth_ = 1;
                    on_stream_stop(stream->cloud_name(), proto::SR_RECORD);
                }

                record_by_event_ = false;
                stream_by_event_ = false;

                // Disable all record triggers
                for (auto& es : event_streams_) {
                    es->set_trigger_recording(false, pre_record_time_.count(),
                                              post_record_time_.count());
                }

                // Stop record
                recording_status_[stream].by_event = false;
                _stop_recording(stream);
            }
            break;
        default:
            logger->error("Stop INVALID");
            return false;
    }

    logger->info("Stream {} stop OK active sinks {} record_by_event {}",
                 stream->cloud_name(), active_sinks_, record_by_event_);

    if (!active_sinks_ &&
        (!record_by_event_ ||
         (record_stream_ && !record_stream_->record_needs_source()))) {
        logger->debug("No sinks, stopping source");
        stream->stop();
    }

    return true;
}

bool manager::on_get_stream_caps(proto::stream_caps& caps) {
    bool result = true;

    // FIXME:
    for (auto& s : streams_) {
        result |= s->get_stream_caps(caps);
    }

    return result;
}

bool manager::on_get_supported_streams(
    proto::supported_streams_config& supportedStreamsConfig) {
    logger->info("Get supported streams");
    std::vector<std::string> namesVideoES, namesAudioES;

    for (auto& s : streams_) {
        proto::supported_stream_config supported_stream;

        if (!s->get_supported_stream(supported_stream)) {
            logger->error("Unable to get supported stream config for stream {}",
                          s->name());
            return false;
        }

        if (!__is_unset(supported_stream.id)) {
            supportedStreamsConfig.streams.push_back(supported_stream);

            if (!__is_unset(supported_stream.video))
                supportedStreamsConfig.video_es.push_back(
                    supported_stream.video);
            if (!__is_unset(supported_stream.audio))
                supportedStreamsConfig.audio_es.push_back(
                    supported_stream.audio);
        }
    }

    // Delete duplicate names
    std::sort(supportedStreamsConfig.video_es.begin(),
              supportedStreamsConfig.video_es.end());
    supportedStreamsConfig.video_es.erase(
        unique(supportedStreamsConfig.video_es.begin(),
               supportedStreamsConfig.video_es.end()),
        supportedStreamsConfig.video_es.end());
    std::sort(supportedStreamsConfig.audio_es.begin(),
              supportedStreamsConfig.audio_es.end());
    supportedStreamsConfig.audio_es.erase(
        unique(supportedStreamsConfig.audio_es.begin(),
               supportedStreamsConfig.audio_es.end()),
        supportedStreamsConfig.audio_es.end());

    return true;
}

bool manager::on_update_preview(std::string url) {
    proto::event_object::snapshot_info_object info;
    info.width = profile::global::instance().default_snapshot_width;
    info.height = profile::global::instance().default_snapshot_height;

    if (snapshot_stream_ == nullptr) {
        logger->warn("No snapshot stream for preview");
        return false;
    }

    if (preview_uploading) {
        logger->warn("New preview request before previouse is not finished!");
        return false;
    }

    if (!snapshot_stream_->get_snapshot(info)) {
        logger->error("Unable to get preview snapshot");
        return false;
    }

    auto req = std::make_shared<request>(url, "POST", info.image_data);

    logger->info("Uploading preview to {}", url);

    req->set_response_cb([=](std::string& resp, int code) {
        if (code == 200) {
            logger->info("Preview successfully uploaded", code);
        } else {
            logger->error("Preview upload failed with code {}", code);
            logger->error("Response: {}", resp.c_str());
        }
        preview_uploading = false;
    });
    preview_uploading = true;

    return http_->make(req);
}

bool manager::on_cam_upgrade_firmware(std::string url) {
    auto req = std::make_shared<request>(url, "GET");

    req->set_response_cb([=](std::string& resp, int code) {
        if (code == 200) {
            logger->info("Firmware of {} bytes successfully downloaded",
                         resp.size());
            callback_->on_cam_upgrade_firmware(resp);
        } else {
            logger->error("Firmware download failed with code {}", code);
            logger->error("Response: {}", resp.c_str());
        }
    });

    return http_->make(req);
}

bool manager::on_get_log() {
    std::string log_data;
    if (callback_)
        callback_->on_get_log(log_data);

    auto req = std::make_shared<request>(
        cm_config_.buildUrl(manager_config::UT_UPLOAD_TEXT), "POST", log_data);

    req->set_response_cb([=](std::string& resp, int code) {
        if (code == 200) {
            logger->info("Log successfully uploaded", code);
        } else {
            logger->error("Log upload failed with code {}", code);
            logger->error("Response: {}", resp.c_str());
        }
    });

    return http_->make(req);
}

bool manager::on_set_stream_by_event(proto::stream_by_event_config conf) {
    auto s = lookup_stream(conf.stream_id);

    pre_record_time_ = std::chrono::milliseconds(conf.pre_event);
    post_record_time_ = std::chrono::milliseconds(conf.post_event);

    if (!s) {
        logger->error("Unknown stream {}", conf.stream_id);
        return false;
    }
    record_stream_ = s;

    logger->info("Event recording set: pre {} post {} for stream {}",
                 conf.pre_event, conf.post_event, conf.stream_id);

    return true;
}

bool manager::on_get_stream_by_event(proto::stream_by_event_config& conf) {
    if (streams_.empty()) {
        logger->warn("No streams for event driven recording");
        return false;
    }

    if (record_stream_ == nullptr) {
        logger->info(
            "No stream for event driven recording was set yet, reporting first "
            "available {}",
            streams_[0]->cloud_name());
        record_stream_ = streams_[0];
    }

    conf.stream_id = record_stream_->cloud_name();
    conf.pre_event = pre_record_time_.count();
    conf.post_event = post_record_time_.count();

    using namespace std::chrono;
    conf.caps.post_event_max =
        duration_cast<milliseconds>(
            profile::global::instance().default_pre_record_time)
            .count();
    conf.caps.pre_event_max =
        duration_cast<milliseconds>(
            profile::global::instance().default_pre_record_time)
            .count();

    return true;
}

bool manager::on_audio_file_play(std::string url) {
    auto req = std::make_shared<request>(url, "GET");
    proto::audio_file_format aff = proto::AFF_INVALID;
    std::string filename;
    auto uri = Uri::Parse(url);

    if (utils::string_endswith(url, ".au"))
        aff = proto::AFF_AU_G711U;
    else if (utils::string_endswith(url, ".wav"))
        aff = proto::AFF_WAV_PCM;
    else if (utils::string_endswith(url, ".mp3"))
        aff = proto::AFF_MP3;
    else {
        logger->error("Unable to determinate audio file format from url");
        return false;
    }

    filename = utils::string_split(uri.Path, '/').back();

    logger->info("Downloading audio file {} of type {}.", filename, aff);

    std::weak_ptr<request> weak_req(req);
    req->set_response_cb([=](std::string& resp, int code) {
        double speed = 0;
        double time = 0;

        if (auto r = weak_req.lock()) {
            speed = r->download_speed / (double)1024;
            time = std::chrono::duration_cast<std::chrono::milliseconds>(
                       r->time_stop - r->time_start)
                       .count() /
                   (double)1000;
        }

        if (code == 200) {
            logger->info(
                "Audio file {} of {} bytes successfully downloaded in {:.3f} "
                "seconds with speed {:.3f} KB/s",
                filename, resp.size(), time, speed);

            callback_->on_audio_file_play(std::move(resp), filename);
        } else {
            logger->error("Audio file download error with code {}", code);
            logger->error("Response: {}", resp);
        }
    });

    return http_->make(req);
}

bool manager::on_trigger_event(std::string event, json meta, cloud::time time) {
    logger->info("Event {} triggered with time {} meta '{}'", event,
                 utils::time::to_iso(time), meta.dump());

    // Construct json representation of the triggered event
    json jevent;
    jevent["event"] = event;

    // Convert json event to event object and find it's configuration and caps
    proto::event_object triggered_event = jevent;
    triggered_event.meta = meta;
    triggered_event.time = utils::time::to_double(time);

    vxg::cloud::agent::proto::event_config event_conf;
    if (events_config_.get_event_config(triggered_event, event_conf)) {
        // If event was specified as supported one and triggerable in the
        // on_get_cam_events_config() and wasn't disabled by user through the
        // Cloud API we call user code and if user returned true we notify this
        // event to the Cloud, user may adjust event's fields such as time or
        // snapshot for instance.
        if (event_conf.caps.trigger && event_conf.active) {
            auto event_stream = _lookup_event_stream_by_event(triggered_event);

            // If event provided by event_stream we trigger it via the
            // corresponding event_stream
            if (event_stream != nullptr)
                return event_stream->trigger_event(triggered_event);

            // If event provided not by the event stream but by the common
            // callback we trigger it via the common callback
            if (callback_->on_trigger_event(triggered_event))
                return notify_event(triggered_event);
        }
    } else {
        logger->error(
            "Triggered unknown event {}, custom events should be specified with"
            " the agent_callback::on_get_cam_events_config()",
            event);
    }

    return false;
}

bool manager::on_raw_message(std::string client_id, std::string& data) {
    return callback_->on_raw_msg(client_id, data);
}

bool manager::on_start_backward(std::string& url) {
    cm_config_.backward_url = url;

    return callback_->on_start_backward_audio(
        cm_config_.buildUrl(manager_config::UT_STREAM_BACKWARD));
}

bool manager::on_stop_backward(std::string& url) {
    return callback_->on_stop_backward_audio(url);
}

bool manager::on_set_timezone(std::string time_zone_str) {
    return callback_->on_set_timezone(time_zone_str);
}

bool manager::on_get_timezone(std::string& time_zone_str) {
    return callback_->on_get_timezone(time_zone_str);
}

// SD recording synchronization
bool manager::on_get_cam_memorycard_timeline(
    proto::command::cam_memorycard_timeline& timeline) {
    if (record_stream_) {
        auto records = record_stream_->record_get_list(
            utils::time::from_iso_packed(timeline.start),
            utils::time::from_iso_packed(timeline.end), false);

        for (auto& r : records) {
            proto::command::memorycard_timeline_data data;
            data.start = utils::time::to_iso_packed(r.tp_start);
            data.end = utils::time::to_iso_packed(r.tp_stop);
            timeline.data.push_back(data);
        }

        // Always return success even if no records?
        return true;
    }

    logger->warn("Unable to get memorycard timeline, no record stream!");
    return false;
}

bool manager::on_cam_memorycard_synchronize(
    proto::command::cam_memorycard_synchronize_status& synchronize_status,
    vxg::cloud::time start,
    vxg::cloud::time end) {
    proto::event_object timeline_sync_request;
    timeline_sync_request.event = proto::ET_CUSTOM;
    timeline_sync_request.custom_event_name = "timeline-sync";
    timeline_sync_request.time = utils::time::to_double(start);
    timeline_sync_request.time_end = utils::time::to_double(end);
    timeline_sync_request.upload_token = synchronize_status.request_id;

    events_states_[timeline_sync_request.name()].start(
        event_state::SDM_UPLOAD, timeline_sync_request, std::chrono::seconds(0),
        std::chrono::seconds(0), record_stream_, synchronize_status.request_id);

    return true;
}

// TODO: Do we really need to check if we have this upload to cancel here?
bool manager::on_cam_memorycard_synchronize_cancel(
    const std::string& request_id) {
    if (uploader_)
        uploader_->cancel_upload(request_id);
    return true;
}

bool manager::_start_recording(media::stream::ptr stream) {
    logger->info(
        "Recording start: permanent: {}, by_event: {}, direct_upload: {}, "
        "started: {}, memorycard: {}",
        recording_status_[stream].permanent, recording_status_[stream].by_event,
        recording_status_[stream].direct_upload,
        recording_status_[stream].started, json(memorycard_info_.status));

    if (memorycard_info_.status != proto::MCS_NORMAL) {
        logger->warn("Storage not ready, no way to start recording!");
        return false;
    }

    if ((recording_status_[stream].permanent ||
         recording_status_[stream].by_event ||
         recording_status_[stream].direct_upload) &&
        !recording_status_[stream].started) {
        recording_status_[stream].started += stream->start_record();
        recording_status_[stream].last_start_time = cloud::time::clock::now();
        // TODO: we need to track recording stream config
    } else {
        logger->info("Recording start not required");
    }
    return recording_status_[stream].started;
}

bool manager::_stop_recording(media::stream::ptr stream, bool force) {
    logger->info(
        "Recording stop: permanent: {}, by_event: {}, direct_upload: {}, "
        "started: {}",
        recording_status_[stream].permanent, recording_status_[stream].by_event,
        recording_status_[stream].direct_upload,
        recording_status_[stream].started);
    if (((!recording_status_[stream].permanent &&
          !recording_status_[stream].by_event &&
          !recording_status_[stream].direct_upload) &&
         recording_status_[stream].started) ||
        force) {
        stream->stop_record();
        logger->info("Recording {}stopped", force ? "force " : "");
        recording_status_[stream].started = false;
    } else {
        logger->info("Recording stop not required");
    }
    return !recording_status_[stream].started;
}

bool manager::on_cam_memorycard_recording(const std::string& stream_id,
                                          bool enabled) {
    auto stream = lookup_stream(stream_id);

    if (stream != nullptr) {
        recording_status_[stream].permanent = enabled;
        recording_status_[stream].stream_id = stream_id;

        if (enabled) {
            auto ret = _start_recording(stream);
            logger->info("Permanent recording {}started!", ret ? "" : "NOT ");
        } else {
            auto ret = _stop_recording(stream);
            logger->info("Permanent recording {}stopped!", ret ? "" : "NOT ");
        }
    }

    return false;
}

void manager::on_registered(const std::string& sid) {
    callback_->on_registered(sid);
}

bool manager::on_set_audio_detection(
    const proto::audio_detection_config& conf) {
    return callback_->on_set_audio_detection(conf);
}

bool manager::on_get_audio_detection(proto::audio_detection_config& conf) {
    return callback_->on_get_audio_detection(conf);
}

// FIXME: Functions below need refactoring/deletion
// FIXME: Functions below need refactoring/deletion
// FIXME: Functions below need refactoring/deletion
bool manager::on_set_log_enable(bool bEnable) {
    return false;
}

bool manager::on_set_activity(bool bEnable) {
    return false;
}

}  // namespace agent
}  // namespace cloud
}  // namespace vxg
