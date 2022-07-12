#pragma once

#include <utils/logging.h>

#include <agent/stream.h>
#include <cloud/CloudRegToken.h>
#include <cloud/Uri.h>
#include <utils/profile.h>
#include <utils/utils.h>

#include <agent/event-manager.h>
#include <agent/timeline-synchronizer.h>

namespace vxg {
namespace cloud {
namespace agent {
struct stats {
    std::chrono::time_point<std::chrono::steady_clock> __spawn_time_;

    stats() { __spawn_time_ = std::chrono::steady_clock::now(); }
    int motion_events {0};

    int records_uploaded {0};
    int records_upload_failed {0};
    size_t records_uploading {0};
    int records_upload_queued {0};
    double records_last_upload_speed_KBps {0.0};

    size_t snapshots_uploading {0};
    int snapshots_upload_failed {0};
    int snapshots_uploaded {0};
    int snapshots_capture_failed {0};
    double snapshot_last_upload_speed_KBps {0.0};

    size_t file_meta_uploading {0};
    int file_meta_upload_failed {0};
    int file_meta_uploaded {0};
    int file_meta_capture_failed {0};
    double file_meta_last_upload_speed_KBps {0.0};

    int cloud_reconnects {0};
    size_t uptime;
    size_t ram_usage_overall_percents {0};
    size_t ram_usage_agent_percents {0};
    size_t cpu_usage_overall_percents {0};
    size_t cpu_usage_agent_percents {0};

    JSON_DEFINE_TYPE_INTRUSIVE(stats,
                               motion_events,
                               snapshots_uploaded,
                               snapshots_capture_failed,
                               records_uploaded,
                               snapshots_upload_failed,
                               records_upload_failed,
                               records_upload_queued,
                               records_uploading,
                               snapshots_uploading,
                               records_last_upload_speed_KBps,
                               snapshot_last_upload_speed_KBps,
                               file_meta_uploading,
                               file_meta_upload_failed,
                               file_meta_uploaded,
                               file_meta_capture_failed,
                               file_meta_last_upload_speed_KBps,
                               cloud_reconnects,
                               uptime,
                               ram_usage_overall_percents,
                               ram_usage_agent_percents,
                               cpu_usage_overall_percents,
                               cpu_usage_agent_percents);
};

//! @brief Cloud agent config
struct config : public event_manager::config, public synchronizer::config {
    //! Device vendor.
    std::string device_vendor {"noname"};
    //! Device model.
    std::string device_model {"nomodel"};
    //! Device serial number.
    std::string device_serial {"noserial"};
    //! Device firmware version.
    std::string device_fw_version {"noversion"};
    //! Device type.
    std::string device_type {"notype"};
    //! Device IP address, used for info purposes only
    std::string device_ipaddr {"127.0.0.1"};
    //! Agent application version.
    std::string agent_version {"noversion"};

    //! Agent application's VXG Cloud Raw Messages support flag.
    bool raw_messaging {false};
    //! Secure connection flag. If this flag set to false cloud::agent will
    //! be using insecure http/rtmp connection to the cloud.
    bool insecure_cloud_channel {true};

    //! Validate SSL certificates for all connections initiated by the agent
    //! library.
    bool allow_invalid_ssl_certs {false};

    //! @brief Default image width for preview and events snapshots.
    //!
    //! Used as suggestion for the [stream::get_snapshot()](@ref
    //! media::stream::get_snapshot) and can be ignored but
    //! [stream::get_snapshot()](@ref media::stream::get_snapshot) must fill
    //! it with the correct value.
    //!
    //! Not used in case of requesting snapshot for proto::event_object
    //! passed via manager::notify_event() with pre-filled snapshot's width.
    int default_snapshot_width {800};

    //! @brief Default snapshot image height.
    //! @see default_snapshot_width
    int default_snapshot_height {600};

    //! @brief by event recording segment duration
    std::chrono::seconds record_by_event_upload_step {std::chrono::seconds(15)};
    //! @brief Max upload speed for recorded videos and snapshots in bytes/s
    size_t max_upload_speed {0};
    //! @brief Max concurent videos upload
    size_t max_concurent_video_uploads {2};
    //! @brief Video upload TTL. Due to low connection speed we may have
    //! situation when upload queue is overwhelmed, this setting restricts
    //! the planned upload's lifetime, all late planned uploads will be
    //! dropped.
    std::chrono::minutes max_video_uploads_queue_lateness {
        std::chrono::minutes(30)};
    //! @brief Max concurent snapshots upload
    size_t max_concurent_snapshot_uploads {4};
    //! @brief Max concurent file_meta upload
    size_t max_concurent_file_meta_uploads {6};
    //! @brief Default event pre recording time
    std::chrono::seconds default_pre_record_time {std::chrono::seconds(10)};
    //! @brief Default event post recording time
    std::chrono::seconds default_post_record_time {std::chrono::seconds(10)};
    //! @brief Max event pre recording time
    std::chrono::seconds max_pre_record_time {std::chrono::seconds(20)};
    //! @brief Max event post recording time
    std::chrono::seconds max_post_record_time {std::chrono::seconds(20)};

    //! @brief Max duration of the /storage/memory_card/{CAMID}/synchronize/
    //! request in seconds.
    std::chrono::seconds max_memorycard_sync_request_duration {60};
    //! @brief Delay between event and start of the corresponding records
    //! upload.
    std::chrono::seconds delay_between_event_and_records_upload_start {60};
    //! @brief
    //!
    std::string cm_registration_sid;

    JSON_DEFINE_TYPE_INTRUSIVE(config,
                               device_vendor,
                               device_model,
                               device_serial,
                               device_fw_version,
                               agent_version,
                               default_snapshot_width,
                               default_snapshot_height);
};
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
