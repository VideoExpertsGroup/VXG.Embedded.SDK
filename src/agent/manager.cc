//  Copyright © 2022 VXG Inc. All rights reserved.
//  Contact: https://www.videoexpertsgroup.com/contact-vxg/
//  This file is part of the demonstration of the VXG Cloud Platform.
//
//  Commercial License Usage
//  Licensees holding valid commercial VXG licenses may use this file in
//  accordance with the commercial license agreement provided with the
//  Software or, alternatively, in accordance with the terms contained in
//  a written agreement between you and VXG Inc. For further information
//  use the contact form at https://www.videoexpertsgroup.com/contact-vxg/

#include <nlohmann/json.hpp>

#include <agent/manager.h>
#include <agent/upload.h>
#include <net/websockets.h>
#include <utils/base64.h>
#include <utils/utils.h>

#include <agent/direct-upload-storage.h>
#include <agent/stream-storage.h>

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

manager::manager(const agent::config& config,
                 const proto::access_token& access_token,
                 callback::ptr callback,
                 transport::worker::ptr proto_transport = nullptr,
                 transport::libwebsockets::http::ptr http_transport = nullptr)
    : command_handler(config, access_token, proto_transport),
      callback_(std::move(callback)),
      started_(false),
      config_(config),
      http_ {http_transport == nullptr
                 ? std::make_shared<transport::libwebsockets::http>(
                       config.allow_invalid_ssl_certs)
                 : http_transport},
      pre_record_time_ {config.default_pre_record_time},
      post_record_time_ {config.default_post_record_time} {
    if (!callback_)
        throw std::runtime_error("Callback should not be null!!!");

    registered_ = false;
    hello_received_ = false;
    auto_reconnect_ = true;
}

manager::~manager() {
    stop();
}

manager::ptr manager::create(const agent::config& config,
                             callback::ptr callback,
                             const proto::access_token& access_token,
                             std::vector<stream::ptr> media_streams,
                             std::vector<event_stream::ptr> event_streams) {
    if (callback == nullptr) {
        vxg::logger::error("Callback should not be nullptr");
        return nullptr;
    }

    struct make_shared_enabler : public manager {
        make_shared_enabler(const agent::config& conf,
                            const proto::access_token& access_token,
                            callback::ptr c)
            : manager(conf, access_token, std::move(c)) {}
    };
    auto result = std::make_shared<make_shared_enabler>(config, access_token,
                                                        std::move(callback));
    if (result != nullptr) {
        result->set_streams(media_streams);
        result->set_event_streams(event_streams);
        return result;
    }

    vxg::logger::critical("Failed to create manager, no memory?");

    return nullptr;
}

void manager::set_streams(std::vector<stream::ptr> streams) {
    streams_ = streams;

    if (streams_.empty()) {
        logger->warn(
            "No media streams were provided by user, working in an idle mode!");
    }

    stream_manager_.set_streams(streams);

    // for (auto& s : streams) {
    // FIXME:
    // std::weak_ptr<stream> w(s);
    // s->set_on_error_cb([this, w](vxg::media::Streamer::StreamError e) {
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

void manager::set_event_streams(std::vector<event_stream::ptr> streams) {
    event_streams_ = streams;

    if (event_streams_.empty())
        logger->warn(
            "No event streams were provided by user, events and recording by "
            "event will not be working!");
}

bool manager::start() {
    websocket::ptr ws = dynamic_pointer_cast<websocket>(transport_);
    // Bind callbacks, bind gives no need to pass a userdata
    ws->set_connected_cb(std::bind(&command_handler::on_connected, this));
    ws->set_disconnected_cb(std::bind(&command_handler::on_disconnected, this));
    ws->set_error_cb(std::bind(&command_handler::on_error, this,
                               placeholders::_1, placeholders::_2));

    if (!proto_config_.proxy_socks5_uri_.empty()) {
        ws->set_proxy(proto_config_.proxy_socks5_uri_);
        http_->set_proxy(proto_config_.proxy_socks5_uri_);
    }

    // Start libwebsockets service threads for HTTP and WebSockets
    http_->start();
    transport_->start();

    // Connection to the cam websocket server, connection attempt failure will
    // be handled by command_proto::on_error and command_proto::on_disconnect
    try_connect();

    auto local_timeline = std::make_shared<sync::timeline>(std::move(
        std::make_shared<stream_storage>(stream_manager_.recording_stream())));
    auto remote_timeline = std::make_shared<sync::timeline>(
        std::move(std::make_shared<direct_upload_proto_storage>(
            proto_config_.access_token_, http_,
            std::bind(&manager::_request_direct_upload_async_storage_item, this,
                      std::placeholders::_1))));

    synchronizer_ =
        synchronizer::create(config_, local_timeline, remote_timeline);
    synchronizer_->start();

    event_manager_ = std::make_shared<event_manager>(
        config_, event_streams_, shared_from_this(),
        std::bind(&manager::handle_event, this, std::placeholders::_1,
                  std::placeholders::_2));
    event_manager_->start();

    return true;
}

void manager::stop() {
    hello_received_ = false;
    registered_ = false;

    if (synchronizer_) {
        synchronizer_->stop();
        synchronizer_ = nullptr;
    }

    if (event_manager_) {
        event_manager_->stop();
        event_manager_ = nullptr;
    }

    stream_manager_.stop_all_streams();

    if (transport_) {
        transport_->disconnect();
        transport_->stop();
    }
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

    stream_manager_.stop_all_streams();
    // FIXME: es

    // Count accident disconnections
    if (reason == proto::BR_CONN_CLOSE)
        stats_.cloud_reconnects++;

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
            proto_config_.reconnect_server_address.clear();
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
                // Stop stream after the settings were configured to apply them
                // The cloud restarts disconnected stream
                // We need to restart only currently used as live stream and
                // don't do anything if it's another stream
                if (s == stream_manager_.live_stream()) {
                    stream_manager_.stop_live();

                    // Stop continuos recording via live if it was started
                    if (sync_mode_ == timeline_sync_mode::RECORD_RTMP_PUBLISH)
                        stream_manager_.stop_live();
                }

                // Restart recording
                if (s == stream_manager_.recording_stream()) {
                    // Save permanent recording status, mark all recordings as
                    // false to stop recording, then we restore the saved
                    // permanent recording status and start everything again
                    auto saved_modes =
                        stream_manager_.recording_status().mode_flags;
                    auto saved_stream_id =
                        stream_manager_.recording_status().stream_id;
                    stream_manager_.stop_recording(saved_modes);
                    stream_manager_.start_recording(saved_stream_id,
                                                    saved_modes);
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

bool manager::handle_event_snapshot(proto::event_object& event) {
    proto::event_config eventConfig;

    // We do not concurrently upload more snapshots than allowed
    // by the user for preventing OOM
    if (stats_.snapshots_uploading >= config_.max_concurent_snapshot_uploads) {
        stats_.snapshots_upload_failed++;
        logger->warn(
            "No space in snapshot upload queue, currently uploading {} >= max "
            "allowed {}, generating event without snapshot!",
            stats_.snapshots_uploading, config_.max_concurent_snapshot_uploads);
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

        event.snapshot_info.width = __is_unset(event.snapshot_info.width)
                                        ? config_.default_snapshot_width
                                        : event.snapshot_info.width;
        event.snapshot_info.height = __is_unset(event.snapshot_info.height)
                                         ? config_.default_snapshot_height
                                         : event.snapshot_info.height;

        if (stream_manager_.snapshot_stream()) {
            if (!stream_manager_.snapshot_stream()->get_snapshot(
                    event.snapshot_info)) {
                logger->warn("Unable to get snapshot");
                stats_.snapshots_capture_failed++;
            } else {
                // VXG Cloud doc states we have to use event trigger time as
                // an image time
                event.snapshot_info.image_time = utils::time::to_iso_packed(
                    utils::time::from_double(event.time));
                return true;
            }
        } else {
            stats_.snapshots_capture_failed++;
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
        if (stats_.file_meta_uploading >=
            config_.max_concurent_file_meta_uploads) {
            stats_.file_meta_upload_failed++;
            logger->trace(
                "No space in file meta upload queue, currently uploading {} >= "
                "max allowed {}, generating event without file meta!",
                stats_.file_meta_uploading,
                config_.max_concurent_file_meta_uploads);
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
    if (!on && stream_manager_.live_status().record_event_depth == 0)
        return false;

    stream_manager_.live_status().record_event_depth += (on ? 1 : -1);

    logger->debug("Stream {} record_event_depth {} on {}", stream_id,
                  stream_manager_.live_status().record_event_depth, on);

    if ((on && stream_manager_.live_status().record_event_depth == 1) ||
        (!on && stream_manager_.live_status().record_event_depth == 0)) {
        event_command.event = proto::ET_RECORD;
        event_command.time = utils::time::to_double(utils::time::now());
        event_command.status = proto::ES_OK;

        // Inform Cloud about record should be started
        event_command.record_info.on = on;
        event_command.record_info.stream_id = stream_id;

        // Append camera id
        event_command.cam_id = proto_config_.cam_id;

        return send_command(json(event_command));
    }

    return false;
}

bool manager::handle_event(proto::event_object& event, bool need_snapshot) {
    using namespace std::chrono;
    bool need_meta = false;
    proto::command::cam_event::ptr event_command =
        dynamic_pointer_cast<cam_event>(factory::create(CAM_EVENT));

    if (!registered_) {
        logger->warn(
            "Unable to notify the event until we aren't connected to the "
            "server!");
        return false;
    }

    /* check event info_ struct if presented/required */
    switch (event.event) {
        case proto::ET_MOTION:
            if (config_.attach_qos_report_to_motion) {
                event.meta = stats_;
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
            if (stream_manager_.recording_stream() &&
                event.memorycard_info.status == proto::MCS_NORMAL &&
                event.memorycard_info.recording_status !=
                    alter_bool::B_INVALID) {
                bool force_stop = false;
                if (stream_manager_.recording_status().started &&
                    !event.memorycard_info.recording_status) {
                    // We need to stop recording inside of the manager to keep
                    // recording state tracking
                    stream_manager_.recording_status().external_stop_count++;
                    force_stop = true;
                    logger->warn(
                        "Recording was stopped from outside {} times already!",
                        stream_manager_.recording_status().external_stop_count);
                }

                stream_manager_.recording_status().started =
                    event.memorycard_info.recording_status;

                logger->info(
                    "Memorycard event notified, recording is {}",
                    stream_manager_.recording_status().started ? "on" : "off");
                if (event.memorycard_info.status == proto::MCS_NORMAL) {
                    auto cached_recording_stream_id_ =
                        stream_manager_.recording_status().stream_id;
                    auto cached_recording_mode_ =
                        stream_manager_.recording_status().mode_flags;
                    if (force_stop)
                        stream_manager_.stop_recording(cached_recording_mode_);
                    stream_manager_.start_recording(cached_recording_stream_id_,
                                                    cached_recording_mode_);
                }
            }
        } break;
        case proto::ET_SOUND: {
        } break;
        default:
            logger->info("NYI event {} notified", event.name());
            break;
    }

    direct_upload_payload_map_ptr payload_ptr =
        std::make_shared<direct_upload_payload_map>();
    auto& payload = *payload_ptr.get();
    if (need_snapshot && (need_snapshot = handle_event_snapshot(event))) {
        // Increment currently uploading snapshots counter used to prevent OOM
        stats_.snapshots_uploading++;
        queued_async_storage::async_item_ptr item =
            std::make_shared<queued_async_storage::async_item>(
                period {utils::time::from_double(event.time),
                        utils::time::from_double(event.time)},
                std::move(event.snapshot_info.image_data), [=](bool ok) {
                    stats_.snapshots_upload_failed += !ok;
                    stats_.snapshots_uploaded += ok;
                    // Decrement currently uploading snapshots counter
                    stats_.snapshots_uploading--;
                });
        item->category = proto::command::UC_SNAPSHOT;

        payload[proto::command::UC_SNAPSHOT] = std::move(item);

        logger->debug("Snapshot {} attached as payload to event id {}",
                      event.snapshot_info.image_time.c_str(),
                      event_command->msgid);
    }

    if (!event.state_emulation && (need_meta = handle_event_meta_file(event))) {
        // meta_files_[event_command->msgid] = event.file_meta_info;
        std::vector<uint8_t> vec_file_meta;
        vec_file_meta.insert(vec_file_meta.begin(),
                             event.file_meta_info.data.begin(),
                             event.file_meta_info.data.end());

        queued_async_storage::async_item_ptr item =
            std::make_shared<queued_async_storage::async_item>(
                period {utils::time::from_double(event.time),
                        utils::time::from_double(event.time)},
                std::move(vec_file_meta),
                [=](bool ok) {
                    stats_.file_meta_upload_failed += !ok;
                    stats_.file_meta_uploaded += ok;
                    stats_.file_meta_uploading--;
                },
                [=]() { return false; });
        item->category = proto::command::UC_FILE_META;

        payload[proto::command::UC_FILE_META] = std::move(item);
        stats_.file_meta_uploading++;

        logger->debug(
            "Meta file of {} bytes attached as payload to event id {}",
            event.file_meta_info.size, event_command->msgid);
    }

    event.status = proto::ES_OK;

    *static_cast<proto::event_object*>(event_command.get()) = event;
    // Append camera id
    event_command->cam_id = proto_config_.cam_id;

    // If we have event's payload like snapshot or meta file we need to request
    // the direct upload url.
    if (!payload.empty())
        return _request_direct_upload(payload_ptr, json(*event_command));

    return send_command(json(*event_command));
}

bool manager::on_direct_upload_url(const direct_upload_url_base& direct_upload,
                                   int event_id,
                                   int refid) {
    std::vector<uint8_t> data;
    std::string media_type;
    auto storage_item =
        std::static_pointer_cast<queued_async_storage::async_item>(
            direct_upload.payload_data);

    logger->info("Direct uploading {} id {} size {} time {} -- {}",
                 json(storage_item->category), refid, storage_item->data.size(),
                 utils::time::to_iso(storage_item->begin),
                 utils::time::to_iso(storage_item->end));
    // No copy here, we move resources
    data = std::move(storage_item->data);

    // if (!data.empty() && !direct_upload.is_canceled())
    //     stats_.records_uploading++;

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

            if (code == 200) {
                if (auto r = weak_req.lock()) {
                    logger->info(
                        "{} with id {} successfully uploaded in {:.3f} "
                        "seconds, "
                        "speed {:.3f} KB/s",
                        json(storage_item->category), refid,
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            r->time_stop - r->time_start)
                                .count() /
                            (double)1000,
                        r->upload_speed / (double)1024);

                    if (storage_item->category == proto::command::UC_SNAPSHOT) {
                        stats_.snapshot_last_upload_speed_KBps =
                            std::round((r->upload_speed / (double)1024) *
                                       1000) /
                            1000;
                    } else if (storage_item->category ==
                               proto::command::UC_RECORD) {
                        stats_.records_last_upload_speed_KBps =
                            std::round((r->upload_speed / (double)1024) *
                                       1000) /
                            1000;
                    }
                }
            } else {
                if (auto r = weak_req.lock())
                    logger->error(
                        "{} {} upload failed with code {} after {:.3f} seconds",
                        json(storage_item->category), refid, code,
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            r->time_stop - r->time_start)
                                .count() /
                            (double)1000);
                else
                    logger->error("{} {} upload failed with code {}",
                                  json(storage_item->category), refid, code);

                logger->error("Response: {}", resp.c_str());
            }
        });

        req->max_upload_speed = config_.max_upload_speed;
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

bool manager::on_get_cam_events_config(proto::events_config& config) {
    return event_manager_->get_events(config);
}

bool manager::on_set_cam_events_config(const proto::events_config& config) {
    return event_manager_->set_events(config);
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

bool manager::_update_storage_status() {
    bool result = false;

    logger->debug("Update storage status");

    if (callback_->on_get_memorycard_info(memorycard_info_)) {
        proto::event_object event;
        event.event = proto::ET_MEMORYCARD;
        event.time = utils::time::to_double(utils::time::now());
        event.memorycard_info = memorycard_info_;

        if (stream_manager_.recording_stream()) {
            if (memorycard_info_.recording_status != alter_bool::B_INVALID) {
                stream_manager_.recording_status().started =
                    memorycard_info_.recording_status;
            } else {
                logger->warn("Recording status is unknown!");
            }
        }

        handle_event(event, false);

        result = (memorycard_info_.status == proto::MCS_NORMAL);
    }

    return result;
}

bool manager::on_stream_start(const std::string& stream_id,
                              int publish_sid,
                              proto::stream_reason reason) {
    std::string url = proto_config_.buildUrl(
        proto::config::UT_STREAM_LIVE, stream_id,
        publish_sid >= 0 ? std::to_string(publish_sid) : "");
    stream::ptr stream = stream_manager_.lookup_stream(stream_id);
    bool result = false;
    // Get memory card status from user
    bool storage_ok = false;

    if (!stream) {
        logger->error("Unable to find stream {}", stream_id);
        return false;
    }

    logger->info("Start stream {}, reason {}, publish url {}",
                 stream->cloud_name(), json(reason).dump(), url.c_str());

    switch (reason) {
        case proto::stream_reason::SR_LIVE:
            logger->debug("Start LIVE");

            result = stream_manager_.start_live(stream_id, url);
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
            sync_mode_ = timeline_sync_mode::RECORD_RTMP_PUBLISH;
            result = stream_manager_.start_live(stream_id, url);
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
            if (storage_ok) {
                result = true;

                if (stream->record_needs_source()) {
                    // If source already was started this function returns true
                    if (stream_manager_.start_media_source(stream_id))
                        return false;
                }

                // Event streams
                // TODO: should we notify all event streams here?
                for (auto& es : event_streams_) {
                    es->set_trigger_recording(true, pre_record_time_.count(),
                                              post_record_time_.count());
                }

                sync_mode_ = timeline_sync_mode::BY_EVENT_DIRECT_UPLOAD;
                // Start recording, if we failed the stream_manager_ will be
                // holding our wish to start in future
                stream_manager_.start_recording(
                    stream_id, media::recording_mode_flags::LOCAL_BY_EVENT);
            } else {
                logger->warn(
                    "Storage is not ready for recording, stream by event mode "
                    "will be used.");
                result = true;
                sync_mode_ = timeline_sync_mode::BY_EVENT_RTMP_PUBLISH;
            }
        } break;
        default:
            logger->error("Start INVALID");
            result = false;
    }

    logger->info("Stream {} start {} reason {}", stream->cloud_name(),
                 result ? "OK" : "Failed", json(reason).dump());

    return result;
}

bool manager::on_stream_stop(const std::string& stream_id,
                             proto::stream_reason reason) {
    stream::ptr stream = stream_manager_.lookup_stream(stream_id);

    if (!stream) {
        logger->error("Unable to find stream {}", stream_id);
        return false;
    }

    logger->info("Stop stream {} reason {}", stream->cloud_name(),
                 json(reason).dump().c_str());

    switch (reason) {
        case proto::stream_reason::SR_RECORD:
        case proto::stream_reason::SR_SERVER_BY_EVENT:
            logger->debug("Stop RECORD");
            sync_mode_ = timeline_sync_mode::NONE;
            [[fallthrough]];
        case proto::stream_reason::SR_LIVE:
            logger->debug("Stop LIVE");
            stream_manager_.stop_live();
            break;
        case proto::stream_reason::SR_RECORD_BY_EVENT:
            logger->debug("Stop RECORD BY EVENT");
            if (sync_mode_ != timeline_sync_mode::BY_EVENT_DIRECT_UPLOAD &&
                sync_mode_ != timeline_sync_mode::BY_EVENT_RTMP_PUBLISH)
                break;

            sync_mode_ = timeline_sync_mode::NONE;

            // Disable all record triggers
            for (auto& es : event_streams_) {
                es->set_trigger_recording(false, pre_record_time_.count(),
                                          post_record_time_.count());
            }

            // Stop by_event recording
            stream_manager_.stop_recording(
                media::recording_mode_flags::LOCAL_BY_EVENT);
            break;
        default:
            logger->error("Stop INVALID");
            return false;
    }

    logger->info(
        "Stream {} STOP [reason: {}, timeline-sync-mode: {}, live-sinks: {}, "
        "source_started: {}, rec-started: {}, rec-mode: {:x}, "
        "rec-need-source: {}].",
        stream_id, json(reason), sync_mode_,
        stream_manager_.status(stream_id).live.sinks,
        stream_manager_.status(stream_id).source_started,
        stream_manager_.status(stream_id).recording.started,
        stream_manager_.status(stream_id).recording.mode_flags,
        stream->record_needs_source());

    // Stop media stream source if it's not required anymore
    if (sync_mode_ != timeline_sync_mode::BY_EVENT_RTMP_PUBLISH)
        stream_manager_.check_stop_media_source(stream);

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
    info.width = config_.default_snapshot_width;
    info.height = config_.default_snapshot_height;

    if (stream_manager_.preview_stream() == nullptr) {
        logger->warn("No snapshot stream for preview");
        return false;
    }

    if (preview_uploading) {
        logger->warn("New preview request before previouse is not finished!");
        return false;
    }

    if (!stream_manager_.preview_stream()->get_snapshot(info)) {
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
        proto_config_.buildUrl(proto::config::UT_UPLOAD_TEXT), "POST",
        log_data);

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
    auto s = stream_manager_.lookup_stream(conf.stream_id);

    pre_record_time_ = std::chrono::milliseconds(conf.pre_event);
    post_record_time_ = std::chrono::milliseconds(conf.post_event);

    if (!s) {
        logger->error("Unknown stream {}", conf.stream_id);
        return false;
    }
    stream_manager_.set_stream_for_by_event(conf.stream_id);

    logger->info("Event recording set: pre {} post {} for stream {}",
                 conf.pre_event, conf.post_event, conf.stream_id);

    return true;
}

bool manager::on_get_stream_by_event(proto::stream_by_event_config& conf) {
    if (streams_.empty()) {
        logger->warn("No streams for event driven recording");
        return false;
    }

    if (stream_manager_.by_event_stream() == nullptr) {
        logger->info(
            "No stream for event driven recording was set yet, reporting first "
            "available {}",
            streams_[0]->cloud_name());
        stream_manager_.set_stream_for_by_event(streams_[0]->cloud_name());
    }

    conf.stream_id = stream_manager_.by_event_stream()->cloud_name();
    conf.pre_event = pre_record_time_.count();
    conf.post_event = post_record_time_.count();

    using namespace std::chrono;
    conf.caps.post_event_max =
        duration_cast<milliseconds>(config_.max_post_record_time).count();
    conf.caps.pre_event_max =
        duration_cast<milliseconds>(config_.max_pre_record_time).count();

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
    return event_manager_->trigger_event(event, meta, time);
}

bool manager::on_raw_message(std::string client_id, std::string& data) {
    return callback_->on_raw_msg(client_id, data);
}

bool manager::on_start_backward(std::string& url) {
    proto_config_.backward_url = url;

    return callback_->on_start_backward_audio(
        proto_config_.buildUrl(proto::config::UT_STREAM_BACKWARD));
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
    if (stream_manager_.recording_stream()) {
        auto records = stream_manager_.recording_stream()->record_get_list(
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
    return !!synchronizer_->sync(
        start, end,
        std::bind(&manager::sync_status_callback, this, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3),
        synchronize_status.request_id);
}

// TODO: Do we really need to check if we have this upload to cancel here?
bool manager::on_cam_memorycard_synchronize_cancel(
    const std::string& request_id) {
    if (synchronizer_)
        synchronizer_->sync_cancel(request_id);
    return true;
}

bool manager::_direct_upload_cmd_from_async_storage_item(
    queued_async_storage::async_item_ptr item,
    proto::command::get_direct_upload_url& request) {
    if (item->state != timed_storage::item::data_state::empty) {
        request.category = item->category;
        request.type = item->media_type;
        request.file_time = utils::time::to_iso_packed(item->begin);
        request.duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(item->end -
                                                                  item->begin)
                .count();
        request.duration_us =
            std::chrono::duration_cast<std::chrono::microseconds>(item->end -
                                                                  item->begin);
        request.size = item->data.size();
        request.stream_id = stream_manager_.recording_stream()->cloud_name();
        request.storage_item = item;

        return true;
    }

    return false;
}

bool manager::_request_direct_upload_async_storage_item(
    queued_async_storage::async_item_ptr item) {
    proto::command::get_direct_upload_url::ptr cmd =
        std::make_shared<proto::command::get_direct_upload_url>();
    direct_upload_payload_map_ptr payload_ptr =
        std::make_shared<direct_upload_payload_map>();
    auto& payload = *payload_ptr.get();

    if (_direct_upload_cmd_from_async_storage_item(item, *cmd)) {
        payload[item->category] = item;

        return _request_direct_upload(std::move(payload_ptr), json(*cmd));
    } else
        item->on_finished(false);

    return false;
}

bool manager::_request_direct_upload(direct_upload_payload_map_ptr payload_ptr,
                                     json cmd) {
    auto timeout = std::chrono::seconds(20);
    if (payload_ptr && cmd != nullptr) {
        cmd["cam_id"] = proto_config_.cam_id;

        // Request upload URL, wait for reply for 20 seconds
        // VXG Cloud has 2 commands for requesting direct upload url:
        //    - cam_event if any of snapshot_info, file_meta_info specified
        //    - get_direct_upload_url
        return send_command_wait_ack(
            cmd,
            [=](bool timedout, proto::command::base_command::ptr ack_cmd,
                std::shared_ptr<void> ack_userdata) {
                auto payload_ptr =
                    std::static_pointer_cast<direct_upload_payload_map>(
                        ack_userdata);
                auto& payload = *payload_ptr.get();

                // If no direct_upload_url was received from the
                // Cloud
                if (timedout) {
                    for (auto& p : payload) {
                        auto async_item = std::static_pointer_cast<
                            queued_async_storage::async_item>(p.second);

                        async_item->on_finished(false);
                        logger->warn(
                            "No reply for {} direct upload request after "
                            "{} seconds, dropping payload for event {}:{}.",
                            json(async_item->category), timeout.count(),
                            cmd["cmd"], (int)cmd["msgid"]);
                    }
                } else {
                    // direct_upload_url command with status OK received
                    if (ack_cmd->cmd == proto::command::DIRECT_UPLOAD_URL &&
                        std::static_pointer_cast<
                            proto::command::direct_upload_url>(ack_cmd)
                                ->status == "OK") {
                        auto cmd = std::static_pointer_cast<
                            proto::command::direct_upload_url>(ack_cmd);
                        queued_async_storage::async_item_ptr item;

                        // Main direct_upload_url response, may not contain
                        // category on old versions of cloud or server, in this
                        // case we always have only one payload data
                        if (cmd->category == proto::command::UC_INVALID)
                            item = std::static_pointer_cast<
                                queued_async_storage::async_item>(
                                payload.begin()->second);
                        else
                            item = std::static_pointer_cast<
                                queued_async_storage::async_item>(
                                payload[cmd->category]);

                        cmd->on_finished = item->on_finished;
                        cmd->is_canceled = item->is_canceled;
                        cmd->payload_data = item;

                        // 'extra' direct_upload_url field, always contains
                        // category
                        for (auto& _cmd : cmd->extra) {
                            item = std::static_pointer_cast<
                                queued_async_storage::async_item>(
                                payload[_cmd.category]);

                            if (__is_unset(_cmd.category))
                                item = std::static_pointer_cast<
                                    queued_async_storage::async_item>(
                                    payload.begin()->second);
                            else
                                item = std::static_pointer_cast<
                                    queued_async_storage::async_item>(
                                    payload[_cmd.category]);

                            _cmd.on_finished = item->on_finished;
                            _cmd.is_canceled = item->is_canceled;
                            _cmd.payload_data = item;
                        }
                        // Here the reply will be passed to
                        // manager::on_direct_upload_url()
                    } else {
                        logger->warn(
                            "Received bad {} reply to direct upload url "
                            "request, marking upload attempt as failed!",
                            ack_cmd->cmd);
                        for (auto& p : payload)
                            std::static_pointer_cast<
                                queued_async_storage::async_item>(p.second)
                                ->on_finished(false);
                    }
                }
            },
            std::move(payload_ptr),
            // Wait for reply with direct upload url for several seconds
            timeout);
    }

    return false;
}

bool manager::on_cam_memorycard_recording(const std::string& stream_id,
                                          bool enabled) {
    auto stream = stream_manager_.lookup_stream(stream_id);

    if (stream != nullptr) {
        if (enabled) {
            auto ret = stream_manager_.start_recording(
                stream_id, media::recording_mode_flags::LOCAL);
            logger->info("Permanent recording {}started!", ret ? "" : "NOT ");
        } else {
            auto ret = stream_manager_.stop_recording(
                media::recording_mode_flags::LOCAL);
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

void manager::sync_status_callback(int progress,
                                   synchronizer::sync_request_status status,
                                   synchronizer::segmenter_ptr seg) {
    if (!seg->ticket.empty()) {
        proto::command::cam_memorycard_synchronize_status status_command;
        status_command.cam_id = proto_config_.access_token_.camid;
        status_command.progress = progress;
        status_command.request_id = seg->ticket;

        vxg::logger::info(
            "Timeline sync request status {}, id {}, done for {}%", status,
            seg->ticket, progress);

        switch (status) {
            case synchronizer::sync_request_status::ERROR:
                status_command.status = MCSS_ERROR;
                break;
            case synchronizer::sync_request_status::DONE:
                status_command.status = MCSS_DONE;
                break;
            case synchronizer::sync_request_status::PENDING:
                status_command.status = MCSS_PENDING;
                break;
            case synchronizer::sync_request_status::CANCELED:
                status_command.status = MCSS_CANCELED;
                break;
            default:
                break;
        }

        send_command(json(status_command));
    }
}

std::shared_ptr<void> manager::on_need_stream_sync_start(
    const event_state& state,
    const cloud::time& start) {
    auto sync_req = std::make_shared<sync_request>();

    logger->info(
        "Timeline sync request start [time: {}, event: {}, sync-mode: {}]",
        utils::time::to_iso(start), state.config().name(), sync_mode_);

    sync_req->sync_mode = sync_mode_;
    if (stream_manager_.recording_stream())
        sync_req->sync_stream_id =
            stream_manager_.recording_stream()->cloud_name();

    // FIXME: event name check looks as not very smart idea
    if (state.config().name() == "timeline-sync") {
        sync_req->sync_mode = sync_mode_;
        sync_req->sync_stop_delay = std::chrono::milliseconds(0);
        sync_req->req = synchronizer_->sync(
            start, utils::time::null(),
            std::bind(&manager::sync_status_callback, this,
                      std::placeholders::_1, std::placeholders::_2,
                      std::placeholders::_3));
    } else if (sync_req->sync_mode ==
               timeline_sync_mode::BY_EVENT_DIRECT_UPLOAD) {
        sync_req->sync_stop_delay = post_record_time_;
        sync_req->req = synchronizer_->sync(
            start - pre_record_time_, utils::time::null(),
            std::bind(&manager::sync_status_callback, this,
                      std::placeholders::_1, std::placeholders::_2,
                      std::placeholders::_3),
            "", config_.delay_between_event_and_records_upload_start);
    } else if (sync_req->sync_mode ==
               timeline_sync_mode::BY_EVENT_RTMP_PUBLISH) {
        auto stream_id = sync_req->sync_stream_id;
        sync_req->sync_stop_delay = post_record_time_;

        // Run in tx thread, prevents locking
        transport_->run_on_rx_thread([this, stream_id]() {
            // TODO: Restart live if we failed, probably check status in
            // event_continue()?

            // Notify Cloud it needs to record live
            __notify_record_event(stream_id, true);
            stream_manager_.start_live(
                stream_id, proto_config_.buildUrl(proto::config::UT_STREAM_LIVE,
                                                  stream_id));
        });
    } else if (sync_req->sync_mode == timeline_sync_mode::RECORD_RTMP_PUBLISH) {
        // Do nothing, Cloud recording ongoing via RTMP
    } else if (sync_req->sync_mode == timeline_sync_mode::NONE) {
        logger->info("Sync not required in mode {}!", sync_req->sync_mode);
        return nullptr;
    } else {
        logger->error("Sync request in wrong sync mode {}!",
                      sync_req->sync_mode);
        return nullptr;
    }

    return sync_req;
}

void manager::on_need_stream_sync_stop(const event_state& state,
                                       const cloud::time& stop,
                                       std::shared_ptr<void> userdata) {
    if (!userdata)
        return;

    auto sync_request =
        std::static_pointer_cast<manager::sync_request>(userdata);

    logger->info(
        "Timeline sync request stop [time: {}, event: {}, sync-mode: {}]",
        utils::time::to_iso(stop), state.config().name(),
        sync_request->sync_mode);

    // FIXME: event name check looks as not very smart idea
    if (state.config().name() == "timeline-sync") {
        synchronizer_->sync_finalize(sync_request->req, stop);
    } else if (sync_request->sync_mode ==
               timeline_sync_mode::BY_EVENT_DIRECT_UPLOAD) {
        synchronizer_->sync_finalize(sync_request->req,
                                     stop + sync_request->sync_stop_delay);
    } else if (sync_request->sync_mode ==
               timeline_sync_mode::BY_EVENT_RTMP_PUBLISH) {
        auto stream_id = sync_request->sync_stream_id;

        // Delayed stop for post record emulation
        transport_->schedule_timed_cb(
            [this, stream_id]() {
                transport_->run_on_rx_thread([this, stream_id]() {
                    __notify_record_event(stream_id, false);
                    // FIXME: it's possible that by_event stream != live_stream
                    stream_manager_.stop_live();
                });
            },
            post_record_time_.count());
    } else if (sync_request->sync_mode ==
               timeline_sync_mode::RECORD_RTMP_PUBLISH) {
        // Do nothing
    } else {
        logger->error("Wrong sync request stop");
    }
}

std::shared_ptr<void> manager::on_need_stream_sync_continue(
    const event_state& state,
    const cloud::time& t,
    std::shared_ptr<void> userdata) {
    logger->info("Sync continuation: event: {}, time: {}, userdata: {}",
                 state.config().name(), utils::time::to_iso_packed(t),
                 static_cast<void*>(userdata.get()));

    // If userdata is not null it means that some sync was already started for
    // this event_state, we need to stop it first
    if (userdata) {
        auto sync_request =
            std::static_pointer_cast<manager::sync_request>(userdata);

        if (sync_request->sync_mode != sync_mode_) {
            logger->info(
                "By event record delivery mode changed {} => {}, switching "
                "sync delivery mode",
                sync_request->sync_mode, sync_mode_);

            switch (sync_request->sync_mode) {
                case timeline_sync_mode::BY_EVENT_RTMP_PUBLISH:
                    // Stopping immediately
                    __notify_record_event(sync_request->sync_stream_id, false);
                    stream_manager_.stop_live();
                    break;
                case timeline_sync_mode::BY_EVENT_DIRECT_UPLOAD:
                    synchronizer_->sync_finalize(sync_request->req, t);
                    break;
                default:
                    break;
            }

            // Unref old userdata
            userdata = nullptr;
        }
    } else {
        if (sync_mode_ == timeline_sync_mode::BY_EVENT_DIRECT_UPLOAD ||
            sync_mode_ == timeline_sync_mode::BY_EVENT_RTMP_PUBLISH) {
            logger->info(
                "By event record delivery mode changed NONE => {}, "
                "switching sync delivery mode",
                sync_mode_);
        }
    }

    // If sync_mode_ is by event and no userdata we need to start sync
    if (!userdata &&
        (sync_mode_ == timeline_sync_mode::BY_EVENT_DIRECT_UPLOAD ||
         sync_mode_ == timeline_sync_mode::BY_EVENT_RTMP_PUBLISH)) {
        // Start new sync from current time and overwrite userdata with new sync
        // request
        userdata = on_need_stream_sync_start(state, t);
    }

    return userdata;
}

void manager::on_event_continue(const event_state& state,
                                const cloud::time& t) {
    logger->info("Event {} ongoing at {}", state.config().name(),
                 utils::time::to_iso_packed(t));
}

}  // namespace agent
}  // namespace cloud
}  // namespace vxg