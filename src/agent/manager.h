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

#pragma once

#include <agent-proto/command-handler.h>
#include <agent/callback.h>
#include <agent/config.h>
#include <agent/event-stream.h>
#include <cloud/CloudShareConnection.h>

#include <agent/stream-manager.h>
#include <agent/stream.h>
#include <agent/upload.h>
#include <net/http.h>
#include <utils/logging.h>

#include <agent/direct-upload-storage.h>
#include <agent/event-manager.h>
#include <agent/timeline-synchronizer.h>

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
class manager : public proto::command_handler,
                public event_manager::event_state_report_cb,
                public std::enable_shared_from_this<manager> {
    vxg::logger::logger_ptr logger = vxg::logger::instance("agent-manager");

private:
    bool started_;

    transport::libwebsockets::http::ptr http_;
    std::vector<std::shared_ptr<agent::media::stream>> streams_;
    media::stream_manager stream_manager_;

    enum class timeline_sync_mode {
        NONE,
        RECORD_RTMP_PUBLISH,
        BY_EVENT_DIRECT_UPLOAD,
        BY_EVENT_RTMP_PUBLISH
    };
    timeline_sync_mode sync_mode_ {timeline_sync_mode::NONE};

    std::chrono::milliseconds pre_record_time_;
    std::chrono::milliseconds post_record_time_;

    std::atomic<bool> preview_uploading {false};
    proto::event_object::memorycard_info_object memorycard_info_;
    callback::ptr callback_ {nullptr};

    synchronizer_ptr synchronizer_;
    event_manager_ptr event_manager_;
    std::vector<agent::event_stream::ptr> event_streams_;
    struct sync_request {
        synchronizer::sync_request_ptr req;
        std::chrono::milliseconds sync_stop_delay;
        timeline_sync_mode sync_mode;
        std::string sync_stream_id;
    };
    using sync_request_ptr = std::shared_ptr<sync_request>;

    virtual std::shared_ptr<void> on_need_stream_sync_start(
        const event_state&,
        const cloud::time& start) override;

    virtual void on_need_stream_sync_stop(const event_state&,
                                          const cloud::time& stop,
                                          std::shared_ptr<void>) override;
    virtual std::shared_ptr<void> on_need_stream_sync_continue(
        const event_state& state,
        const cloud::time& t,
        std::shared_ptr<void> userdata);
    virtual void on_event_continue(const event_state&,
                                   const cloud::time&) override;
    virtual void sync_status_callback(int progress,
                                      synchronizer::sync_request_status status,
                                      synchronizer::segmenter_ptr seg);

    bool _direct_upload_cmd_from_async_storage_item(
        queued_async_storage::async_item_ptr item,
        proto::command::get_direct_upload_url& request);

public:
    using direct_upload_payload_map =
        std::map<proto::upload_category, std::shared_ptr<void>>;
    using direct_upload_payload_map_ptr =
        std::shared_ptr<direct_upload_payload_map>;

private:
    agent::config config_;
    agent::stats stats_;

    bool _request_direct_upload(direct_upload_payload_map_ptr payload,
                                json cmd);
    bool _request_direct_upload_async_storage_item(
        queued_async_storage::async_item_ptr item);

public:
    //! \private
    virtual ~manager();

    //! @brief shared_ptr to manager object
    typedef std::shared_ptr<manager> ptr;
    //!
    //! @brief Create manager object
    //!
    //! @param[in] config
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
        const agent::config& config,
        callback::ptr callback,
        const proto::access_token& access_token,
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
    //! @param[in] config Cloud agent manager's configuration
    //! @param[in] access_token
    //! @param[in] callback
    //! @param[in] transport Used as protocol transport if specified, if nullptr
    //! the library will create own websockets transport.
    //! @param[in] http_transport HTTP transport for HTTP requests to the Cloud.
    //! @private
    //! TODO: create transport::websocket and transport::http abstract classes
    manager(const agent::config& config,
            const proto::access_token& access_token,
            callback::ptr callback,
            transport::worker::ptr proto_transport,
            transport::libwebsockets::http::ptr http_transport);

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

    bool handle_event(proto::event_object& event, bool need_snapshot);

    // Storage status event trigger
    bool _update_storage_status();

    bool handle_event_snapshot(proto::event_object& event);
    bool handle_event_meta_file(proto::event_object& event);
    bool __notify_record_event(std::string stream_id, bool on);
    // FIXME:

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
