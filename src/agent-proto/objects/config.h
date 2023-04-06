#pragma once

#ifndef __CONFIG_H_
#define __CONFIG_H_

#include <iostream>
#include <string>
#include <vector>

#include <config.h>

#include <nlohmann/json.hpp>

#include <agent-proto/command/unset-helper.h>
#include <agent-proto/command/utils.h>
#include <agent-proto/objects/caps.h>

#include <utils/base64.h>
#include <utils/logging.h>
#include <utils/utils.h>

/*! \file config.h VXG Cloud CM protocol objects */

//! \cond
using json = nlohmann::json;
//! \endcond

namespace vxg {
namespace cloud {
//!
//! \brief time point
//!
//!
namespace time_spec {
using precision = std::chrono::nanoseconds;
template <typename T>
using duration = typename std::conditional<std::is_same<T, precision>::value,
                                           precision,
                                           std::chrono::duration<T> >::type;
}  // namespace time_spec
using time =
    std::chrono::time_point<std::chrono::system_clock, time_spec::precision>;
using duration = time_spec::duration<time_spec::precision>;
}  // namespace cloud
}  // namespace vxg

namespace nlohmann {
//! \private
template <>
struct adl_serializer<vxg::cloud::time> {
    static void from_json(const json& j, vxg::cloud::time& c) {
        if (j.is_string()) {
            if (vxg::cloud::utils::time::is_iso(j))
                c = vxg::cloud::utils::time::from_iso2(j);
            else if (vxg::cloud::utils::time::is_iso_packed(j))
                c = vxg::cloud::utils::time::from_iso_packed(j);
            else
                c = UnsetTime;
        } else
            c = UnsetTime;
    }

    static void to_json(json& j, const vxg::cloud::time& c) {
        j = vxg::cloud::utils::time::to_iso2(c);
    }
};
}  // namespace nlohmann

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
#ifndef ignore_exception
//! \cond
#define ignore_exception(...) \
    try {                     \
        __VA_ARGS__;          \
    } catch (...) {           \
    }
//! \endcond
#endif

//!
//! \brief Video stream config
//!
struct video_stream_config {
    //!  Mandatory:
    //!  video ES to use
    std::string stream {UnsetString};

    //!  Mandatory:
    //!  video encoding format
    video_format format {VF_INVALID};

    //!  Optional:
    //!  profile that specifies format, when format assumes it
    std::string profile {UnsetString};

    //! Mandatory:
    //! int (horz) - video resolution width x height
    int horz {UnsetInt};
    //! Mandatory:
    //! int (vert) - video resolution width x height
    int vert {UnsetInt};

    //! Mandatory:
    //! framerate
    double fps {UnsetDouble};

    //! Mandatory:
    //! prefer VBR; if false or not set CBR should be used
    bool vbr {false};

    //! Mandatory:
    //! gop size (I-Frame interval);
    int gop {UnsetInt};

    //! Optional:
    //! bitrate, kbps
    int brt {UnsetInt};

    //! Optional:
    //! bitrate for VBR, kbps
    int vbr_brt {UnsetInt};

    //! Optional:
    //! int [-4..4], quality profile hint for encoder, where 0 means normal
    int quality {UnsetInt};

    //! Optional:
    //! a smoothing value from range 0-100 (%)
    int smoothing {UnsetInt};

private:
    //! \internal nlohmann::json reflection mapper
    JSON_DEFINE_TYPE_INTRUSIVE(video_stream_config,
                               stream,
                               format,
                               profile,
                               horz,
                               vert,
                               fps,
                               vbr,
                               quality,
                               gop,
                               brt,
                               vbr_brt,
                               smoothing);
};
//! \private
inline bool operator==(const video_stream_config& lhs,
                       const video_stream_config& rhs) {
    vxg::logger::instance("video-stream-settings")
        ->trace("{} {}, {} {}, {} {}, {} {}, {} {}, {} {}, {} {}, {} {}, {} {}",
                lhs.stream, rhs.stream, lhs.brt, rhs.brt, lhs.format,
                rhs.format, lhs.fps, rhs.fps, lhs.gop, rhs.gop, lhs.horz,
                rhs.horz, lhs.vert, rhs.vert, lhs.vbr, rhs.vbr, lhs.vbr_brt,
                rhs.vbr_brt);
    // clang-format off
    return ((lhs.stream == rhs.stream)   || (__is_unset(lhs.stream)  || __is_unset(rhs.stream))) &&
           ((lhs.profile == rhs.profile) || (__is_unset(lhs.profile)  || __is_unset(rhs.profile))) &&
           ((lhs.brt == rhs.brt)         || (__is_unset(lhs.brt)     || __is_unset(rhs.brt))) &&
           ((lhs.format == rhs.format)   || (__is_unset(lhs.format)  || __is_unset(rhs.format))) &&
           ((lhs.fps == rhs.fps)         || (__is_unset(lhs.fps)     || __is_unset(rhs.fps))) &&
           ((lhs.gop == rhs.gop)         || (__is_unset(lhs.gop)     || __is_unset(rhs.gop))) &&
           ((lhs.horz == rhs.horz)       || (__is_unset(lhs.horz)    || __is_unset(rhs.horz))) &&
           ((lhs.vert == rhs.vert)       || (__is_unset(lhs.vert)    || __is_unset(rhs.vert))) &&
           ((lhs.vbr == rhs.vbr)         || (__is_unset(lhs.vbr)     || __is_unset(rhs.vbr))) &&
           ((lhs.vbr_brt == rhs.vbr_brt) || (__is_unset(lhs.vbr_brt) || __is_unset(rhs.vbr_brt)));
    // clang-format on
}
//! \private
inline bool operator!=(const video_stream_config& lhs,
                       const video_stream_config& rhs) {
    return !operator==(lhs, rhs);
}

//!
//! \brief Audio media stream config
//!
//!
struct audio_stream_config {
    //! Mandatory:
    //! audio ES to use
    std::string stream {UnsetString};

    //! Mandatory:
    //! audio encoding format
    audio_format format {AF_INVALID};

    //! Mandatory:
    //! bitrate, kbps
    int brt {UnsetInt};

    //! Mandatory:
    //! samplerate, KHz
    double srt {UnsetDouble};

private:
    //! \internal nlohmann::json reflection mapper
    JSON_DEFINE_TYPE_INTRUSIVE(audio_stream_config, stream, format, brt, srt);
};
//! \private
inline bool operator==(const audio_stream_config& lhs,
                       const audio_stream_config& rhs) {
    // clang-format off
    return  ((lhs.stream == rhs.stream) || (__is_unset(lhs.stream) || __is_unset(rhs.stream))) &&
            ((lhs.format == rhs.format) || (__is_unset(lhs.format) || __is_unset(rhs.format))) &&
            ((lhs.brt == rhs.brt)       || (__is_unset(lhs.brt)    || __is_unset(rhs.brt))) &&
            ((lhs.srt == rhs.srt)       || (__is_unset(lhs.srt)    || __is_unset(rhs.srt)));
    // clang-format on
}
//! \private
inline bool operator!=(const audio_stream_config& lhs,
                       const audio_stream_config& rhs) {
    return !operator==(lhs, rhs);
}
//!
//! \brief Media stream config
//!
//!
struct stream_config {
    //! List of video media stream configs
    std::vector<video_stream_config> video;
    //! List of audio media stream configs
    std::vector<audio_stream_config> audio;

private:
    //! \internal nlohmann::json reflection mapper
    JSON_DEFINE_TYPE_INTRUSIVE(stream_config, video, audio);
};

//! Motion detection related structs
//
//! \internal 5.1 get_motion_detection (SRV)
//! \internal 5.2 set_motion_detection (SRV)
//! \internal 5.3 motion_detection_conf (CM)

//!
//! \brief Motion region
//!
//!
struct motion_region {
    //! Mandatory:
    //! name of region if supported by camera
    std::string region {UnsetString};

    //! Mandatory:
    //! String is packed with Apple Packbit algorithm and after that encoded
    //! with Base64. Bitstring where “1” denotes an active cell and a “0” an
    //! inactive cell. The first cell is in the upper left corner. Then the cell
    //! order goes first from left to right and then from up to down. If the
    //! number of cells is not a multiple of 8 the last byte is padded with
    //! zeros.
    std::string map {UnsetString};

    //! Mandatory:
    //! range 0-100; 0 - minimal sensitivity.
    //! If sensitivity is supported only for whole frame, the same value should
    //! be used for all regions.
    size_t sensitivity {0};

    //! Mandatory:
    //! indicates that motion detection is enabled for the region
    bool enabled {false};

private:
    //! \internal nlohmann::json reflection mapper
    JSON_DEFINE_TYPE_INTRUSIVE(motion_region,
                               region,
                               map,
                               sensitivity,
                               enabled);
};

//!
//! \brief Motion detection config
//!
//!
struct motion_detection_config {
    //! Mandatory \internal CM => SRV (reply to 'get_motion_detection')
    //! columns - number of detection cells by X
    int columns {UnsetInt};
    //! Mandatory \internal for CM => SRV (reply to 'get_motion_detection')
    //! rows - number of detection cells by Y
    int rows {UnsetInt};
    //! Mandatory for CM => SRV (reply to 'get_motion_detection')
    //! camera capabilities that limit possible motion detection configuration
    motion_detection_caps caps;
    //! Mandatory
    //! List of motion regions
    std::vector<motion_region> regions;

private:
    //! \internal nlohmann::json reflection mapper
    JSON_DEFINE_TYPE_INTRUSIVE(motion_detection_config,
                               columns,
                               rows,
                               caps,
                               regions);
};

//!
//! \brief Video image config
//!
//! \internal 3.6 get_cam_video_conf (SRV)
//! \internal 3.7 cam_video_conf (CM)
//! \internal 3.8 set_cam_video_conf (SRV)
struct video_config {
    //! vert_flip: optional string, vertical image flip mode: [“off”, “on”,
    //! “auto”]
    std::string vert_flip {UnsetString};

    //! horz_flip: optional string, horizontal image flip mode: [“off”, “on”,
    //! “auto”]
    std::string horz_flip {UnsetString};

    //! tdn: optional string, possible values [“day”, “night”, “auto”]
    std::string tdn {UnsetString};

    //! ir_light: optional string, IR light for night conditions [“off”, “on”,
    //! “auto”]
    std::string ir_light {UnsetString};

    //! brightness: optional int, a brightness value from range 0-100 (%)
    int brightness {UnsetInt};

    //! contrast: optional int, a contrast value from range 0-100 (%)
    int contrast {UnsetInt};

    //! saturation: optional int, a saturation value from range 0-100 (%)
    int saturation {UnsetInt};

    //! sharpness: optional int, a sharpness value from range 0-100 (%)
    int sharpness {UnsetInt};

    //! nr_type: optional string, one of predefined noise reduce types from caps
    std::string nr_type {UnsetString};

    //! nr_level: optional int, level of noise reduce when filter requires it
    //! 0-100 (%)
    int nr_level {UnsetInt};

    //! wb_type: optional string, one of predefined white balance types from
    //! caps
    std::string wb_type {UnsetString};

    //! pwr_frequency: optional int, power line frequency [50, 60] (Hz)
    int pwr_frequency {UnsetInt};

    //! caps
    video_caps caps;

private:
    //! \internal nlohmann::json reflection mapper
    JSON_DEFINE_TYPE_INTRUSIVE(video_config,
                               vert_flip,
                               horz_flip,
                               tdn,
                               ir_light,
                               brightness,
                               contrast,
                               saturation,
                               sharpness,
                               nr_type,
                               nr_level,
                               wb_type,
                               pwr_frequency,
                               caps);
};

//!
//! \brief Event status
//!
//! \internal
//! 6.2 get_cam_events (SRV)
//! 6.3 set_cam_events (SRV)
//! 6.4 cam_events_conf (CM)
//! 6.5 cam_event (CM)
//! \endinternal
enum event_status {
    //! Ok
    ES_OK,
    //! Error
    ES_ERROR,

    //! Default status, invalid
    ES_INVALID = -1,
};

//! \cond
// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( event_status, {
    {ES_INVALID, nullptr},
    {ES_OK, "OK"},
    {ES_ERROR, "ERROR"},
})
// clang-format on
//! \endcond

//!
//! \brief Types of events
//!
enum event_type {
    //! "motion"  for motion detection events
    ET_MOTION,
    //! "sound" for audio detection
    ET_SOUND,
    //! "net" for the camera network status change
    ET_NET,
    //! "record" CM informs server about necessity of changing of recording
    //! state
    ET_RECORD,
    //! "memorycard" camera's memory-card status change
    ET_MEMORYCARD,
    //! "wifi" status of camera's currently used Wi-Fi
    ET_WIFI,
    //! Custom event
    ET_CUSTOM = 0x100,

    //! Invalid event type
    ET_INVALID = -1,
};

//! \cond
// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( event_type, {
    { ET_INVALID, nullptr},
    //! “motion”  for motion detection events
    { ET_MOTION, "motion" },
    //! “sound” for audio detection
    { ET_SOUND, "sound" },
    //! “net” for the camera network status change
    { ET_NET, "net" },
    //! “record” CM informs server about necessity of changing of recording state
    { ET_RECORD, "record" },
    //! “memorycard” camera's memory-card status change
    { ET_MEMORYCARD, "memorycard" },
    //! “wifi” status of camera's currently used Wi-Fi
    { ET_WIFI, "wifi" },
    //! "custom", all new events should use type custom, custom_event_name
    //!  should be used to specify custom event's name
    { ET_CUSTOM, "custom" }
})
// clang-format on
//! \endcond

//!
//! \brief Video recoding(mp4 file) clip description,
//!
//!
struct video_clip_info {
    //! \private
    //! \deprecated Use tp_start
    std::time_t time_begin {UnsetInt};
    //! \private
    //! \deprecated Use tp_stop
    std::time_t time_end {UnsetInt};

    //! Clip start time UTC
    cloud::time tp_start;
    //! Clip stop time UTC
    cloud::time tp_stop;

    //! Clip start time local
    cloud::time local_start;

    //! Clip stop time local
    cloud::time local_stop;

    //! Video clip picture width
    int video_width {UnsetInt};
    //! Video clip picture height
    int video_height {UnsetInt};

    //! Video data buffer, we use move semantics internally so no data copying
    //! will be invoked.
    std::vector<uint8_t> data;
};

//!
//! \brief Memory card status
//!
enum memorycard_status {
    //! No memorycard
    MCS_NONE,
    //! Memorycard is OK
    MCS_NORMAL,
    //! Need formatting
    MCS_NEED_FORMAT,
    //! Formatting ongoing
    MCS_FORMATTING,
    //! Initialization, not mounted yet for example.
    MCS_INITIALIZATION,

    //! Invalid value
    MCS_INVALID = -1,
};

//! \cond
// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( memorycard_status, {
    {MCS_INVALID, nullptr},
    {MCS_NONE, "none"},
    {MCS_NORMAL, "normal"},
    {MCS_NEED_FORMAT, "need-format"},
    {MCS_FORMATTING, "formatting"},
    {MCS_INITIALIZATION, "initialization"},
})
// clang-format on
//! \endcond

//!
//! \internal 3.19 cam_list_wifi (SRV)
//! \internal 3.20 cam_wifi_list (CM)
//! \internal 3.21 cam_set_current_wifi (SRV)

//! \brief WiFi encryption type
//!
enum wifi_encryption {
    //! No encryption
    WFE_OPEN,
    //! WEP
    WFE_WEP,
    //! WPA-PSK
    WFE_WPA,
    //! WPA2-PSK
    WFE_WPA2,
    //! WPA-Enterprise
    WFE_WPA_ENTERPRISE,
    //! WPA2-Enterprise
    WFE_WPA2_ENTERPRISE,

    //! Default, invalid value
    WFE_INVALID = -1,
};

//! \cond
// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( wifi_encryption, {
    {WFE_INVALID, nullptr},
    {WFE_OPEN, "open"},
    {WFE_WEP, "WEP"},
    {WFE_WPA, "WPA"},
    {WFE_WPA2, "WPA2"},
    {WFE_WPA_ENTERPRISE, "WPA_enterprise"},
    {WFE_WPA2_ENTERPRISE, "WPA2_enterprise"},
})
// clang-format on
//! \endcond

//! @ingroup wifi
//! \brief WiFi network object
//!
struct wifi_network {
    //! ssid: string, network SSID
    std::string ssid {UnsetString};
    //! signal: int, signal strength, dB
    int signal {UnsetInt};
    //! mac: string, AP MAC address
    std::string mac {UnsetString};
    //! encryption_caps: list of string, supported encryption types,
    std::vector<wifi_encryption> encryption_caps;
    //! encryption: string, current encryption type, see encryption_caps for
    //! possible values
    wifi_encryption encryption {wifi_encryption::WFE_INVALID};
    //! password: string, network password
    std::string password {UnsetString};

private:
    //! \internal nlohmann::json reflection mapper.
    JSON_DEFINE_TYPE_INTRUSIVE(wifi_network,
                               ssid,
                               signal,
                               mac,
                               encryption_caps,
                               encryption,
                               password);
};

//! @ingroup wifi
//! \brief WiFi config
//!
struct wifi_config {
    //! List of wifi_network objects
    std::vector<wifi_network> networks;

private:
    //! \internal nlohmann::json reflection mapper.
    JSON_DEFINE_TYPE_INTRUSIVE(wifi_config, networks);
};
//! @ingroup wifi
//!
//! \brief wifi_config
//!
//!
typedef wifi_config wifi_list;

//! \brief WiFi connection state
enum wifi_network_state {
    WNS_UNKNOWN,
    WNS_INITIALIZE_0,
    WNS_INITIALIZE_1,
    WNS_TRY_CONNECT,
    WNS_RECEIVING_IP,
    WNS_CONNECTED,

    //! Invalid value
    WNS_INVALID = -1,
};

//! \cond
// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( wifi_network_state, {
    { WNS_UNKNOWN, "unknown"},
    { WNS_INITIALIZE_0, "initialize_0"},
    { WNS_INITIALIZE_1, "initialize_1"},
    { WNS_TRY_CONNECT, "try_connect"},
    { WNS_RECEIVING_IP, "receiving_ip"},
    { WNS_CONNECTED, "connected"},
    { WNS_INVALID, nullptr }
});
// clang-format on

//!
//! \brief Event object
//!
//!
struct event_object {
    //!
    //! @brief Snapshot info
    //!
    //!
    struct snapshot_info_object {
        //! image_time: string, image UTC time in ISO8601 format
        //! YYYYMMDDThhmmss.mmm, should be the same as file_time  in
        //! corresponding upload request. Since the time-stamp is used as an
        //! unique ID of the snapshot, CM MUST ensure uniqueness of the
        //! time-stamp of all snapshots of a particular camera. If this
        //! parameter is accompanied with “width”, “height” and “size”, server
        //! will reply with 3.23 direct_upload_url (SRV) and the URL should be
        //! used for direct uploading of the image. Otherwise an obsolete way
        //! (is not recommended now) of upload can be used (see 6.6 Media files
        //! uploading)
        std::string image_time {UnsetString};
        //! width: optional int. Width of image in pixels
        int width {UnsetInt};
        //! height: optional int. Height of image in pixels
        int height {UnsetInt};
        //! size: optional int. Size of image-file in bytes
        int size {UnsetInt};

        //! jpeg snapshot
        std::vector<uint8_t> image_data;

    private:
        //! \internal nlohmann::json reflection mapper
        JSON_DEFINE_TYPE_INTRUSIVE(snapshot_info_object,
                                   image_time,
                                   width,
                                   height,
                                   size);
    };

    //! \brief Motion info
    struct motion_info_object {
        //! map: optional string, map of cells where motion is detected
        std::string map {UnsetString};
        //! regions: optional list of string, names of regions where motion is
        //! detected
        std::vector<std::string> regions;

    private:
        //! \internal nlohmann::json reflection mapper
        JSON_DEFINE_TYPE_INTRUSIVE(motion_info_object, map, regions);
    };

    //! @brief record_info
    struct record_info_object {
        alter_bool on {alter_bool::B_INVALID};
        std::string stream_id;

    private:
        //! \internal nlohmann::json reflection mapper
        JSON_DEFINE_TYPE_INTRUSIVE(record_info_object, on, stream_id);
    };
    // struct NetInfo {
    //     NLOHMANN_DEFINE_TYPE_INTRUSIVE(NetInfo);
    // };

    //! \brief Memorycard info
    struct memorycard_info_object {
        //! status
        memorycard_status status {MCS_INVALID};
        //! memorycard size in MBs
        uint64_t size {UnsetUInt64};
        //! memorycard free space in MBs
        uint64_t free {UnsetUInt64};
        //! optional int, Max duration that can be requested for storages sync
        //! operation . 0 means - sync is not supported (default value when
        //! parameter is missed). null - unlimited duration can be requested.
        //! Should be included: the first event right after connection
        int max_sync_duration {UnsetInt};
        //! optional bool, current status of memory card recording. Any change
        //! of status must be accompanied with the notification. It also must be
        //! sent in the beginning to report initial status.
        alter_bool recording_status {alter_bool::B_INVALID};

    private:
        //! \internal nlohmann::json reflection mapper
        JSON_DEFINE_TYPE_INTRUSIVE(memorycard_info_object,
                                   status,
                                   size,
                                   free,
                                   max_sync_duration,
                                   recording_status);
    };

    //! @brief meta file info struct
    struct file_meta_info_object {
        int size {UnsetInt};

        std::string data;

    private:
        JSON_DEFINE_TYPE_INTRUSIVE(file_meta_info_object, size);
    };

    struct wifi_info_object : public wifi_network {
        //! state: optional string, indicates connection state of the currently
        //! used network; one of wifi_network_state enum values.
        wifi_network_state state {WNS_INVALID};

    private:
        JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(wifi_info_object,
                                           wifi_network,
                                           state);
    };

    //! @brief Snapshot info
    snapshot_info_object snapshot_info;
    //! @brief Motion info. Used for ET_MOTION.
    motion_info_object motion_info;
    //! @brief record info object, used for system event ET_RECORD
    record_info_object record_info;
    //! @brief meta file info object, if presented the Cloud will reply with
    //!        meta file upload URL.
    file_meta_info_object file_meta_info;
    // //! net_info
    // NetInfo net_info;

    //!
    //! @brief Memorycard info. Used for ET_MEMORYCARD event.
    //!
    memorycard_info_object memorycard_info;
    //! @brief Wifi connection info, must be filled for event ET_WIFI.
    wifi_info_object wifi_info;

    //! event: event_type
    event_type event {event_type::ET_INVALID};
    //! all new custom events should use type event_type::ET_CUSTOM,
    //! event_object.custom_event_name should be used to specify it's name,
    //! this field ignored for standard event types.
    std::string custom_event_name {UnsetString};
    //! time: float,  calendar time, UTC
    double time {UnsetDouble};
    //! mediatm: optional int, media time stamp to synchronize with media stream
    uint64_t mediatm {UnsetUInt64};
    //! contains status of event processing (useful
    //! when an event implies an ambiguous result of processing)
    //! event_status::ES_OK - success. Default value; event_status::ES_ERROR –
    //! camera couldn't process the event well (for example making a snapshot
    //! has been failed). More details can be obtained from field
    //! error_description; error_description: optional string, contains
    //! description of the problem in case of unsuccessful processing of the
    //! event
    event_status status {ES_INVALID};
    //! meta: optional struct, string key-value mapping with some event metadata
    //! (for example, custom data when event is triggered externally or bounding
    //! rectangle from face detector)
    json meta;

    ///! State for stateful events, not part of the Cloud API yet, used for
    ///! statefull events state indication.
    bool active {false};
    //! @internal
    //! Following members are not part of Cloud API but used to extend the
    //! functionality
    //! time_end: float, calendar time, UTC, can be used to specify end time
    //! for stateless events to extend event time interval
    double time_end {UnsetDouble};
    ///! Upload token id, used for memorycard sync API to identify the
    ///! requested upload.
    std::string upload_token;
    bool upload_canceler {false};
    ///! meta_file: raw metadata in any string format, if set will be uploaded
    ///! as meta file
    std::string meta_file {UnsetString};
    //! @brief Stateful event emulation flag.
    //! If set this event object is not real event but state dummy event used to
    //! emulate state on the Cloud.
    bool state_dummy {false};
    //! @endinternal

private:
    //! \cond
    operator std::string() {
        if (event == ET_CUSTOM)
            return custom_event_name;

        return std::to_string(event);
    }

    //! \internal nlohmann::json reflection mapper
    friend void to_json(json& j, const event_object& c) {
        j.update({{"time", c.time}});
        if (!__is_unset(c.mediatm))
            j.update({{"mediatm", c.mediatm}});
        if (!__is_unset(c.status))
            j.update({{"status", c.status}});
        if (!__is_unset(c.meta))
            j.update({{"meta", c.meta}});
        if (!__is_unset(c.snapshot_info.image_time))
            j.update({{"snapshot_info", c.snapshot_info}});
        if (!__is_unset(c.file_meta_info.size))
            j.update({{"file_meta_info", c.file_meta_info}});

        j.update({{"event", c.event}});
        switch (c.event) {
            // “motion”  for motion detection events
            case ET_MOTION:
                j.update({{"motion_info", c.motion_info}});
                break;
            case ET_MEMORYCARD:
                if (!__is_unset(c.memorycard_info.status))
                    j.update({{"memorycard_info", c.memorycard_info}});
                break;
            case ET_RECORD:
                if (!__is_unset(c.record_info.on))
                    j.update({{"record_info", c.record_info}});
                break;
            case ET_WIFI:
                j.update({{"wifi_info", c.wifi_info}});
                break;
            // TODO:
            // // “sound” for audio detection
            // case ET_SOUND:
            // break;
            // // “net” for the camera network status change
            // case ET_NET:
            //     j.update({{"net_info", c.net_info}});
            // break;
            case ET_CUSTOM:
                j.update({{"event", c.custom_event_name}});
                break;
            default: {
            }
        }
    }

    //! \internal nlohmann::json reflection mapper
    friend void from_json(const json& j, event_object& c) {
        ignore_exception(j.at("event").get_to(c.event));
        if (c.event == ET_INVALID) {
            c.event = ET_CUSTOM;
            ignore_exception(j.at("event").get_to(c.custom_event_name));
        }

        ignore_exception(j.at("time").get_to(c.time));
        ignore_exception(j.at("status").get_to(c.status));
        ignore_exception(j.at("meta").get_to(c.meta));

        ignore_exception(j.at("mediatm").get_to(c.mediatm));
        ignore_exception(j.at("snapshot_info").get_to(c.snapshot_info));
        ignore_exception(j.at("motion_info").get_to(c.motion_info));
        ignore_exception(j.at("record_info").get_to(c.record_info));
        ignore_exception(j.at("file_meta_info").get_to(c.file_meta_info));
        ignore_exception(j.at("wifi_info").get_to(c.wifi_info));
        ignore_exception(j.at("memorycard_info").get_to(c.memorycard_info));
        ignore_exception(j.at("record_info").get_to(c.record_info));
        // TODO:
        // ignore_exception(j.at("net_info").get_to(c.net_info));
    }
    //! \endcond

public:
    std::string name() {
        if (event == ET_CUSTOM)
            return custom_event_name;
        return json(event);
    }
};

//!
//! \brief Event config
//!
struct event_config {
    //! event: string, event name, see 6.1 Events naming for details
    event_type event {event_type::ET_INVALID};

    //! Custom event name, used if event set to event_type::ET_CUSTOM.
    std::string custom_event_name {UnsetString};

    //! active: bool, event is active; if not set, corresponding events will not
    //! be sent
    bool active {false};

    //! stream: bool, start stream when event happens
    bool stream {false};

    //! snapshot: bool, generate snapshot when event happens
    bool snapshot {false};

    //! period: optional int, an interval between periodic events, seconds
    int period {UnsetInt};

    //! Event capabilities
    //! \internal
    //! Used as reply to get_cam_events only, ignored for other commands
    //! \endinternal
    event_caps caps;

    //! @brief Is-equal predicate based on event's name only.
    //!
    //! @param r
    //! @return true Compared configs are for the event with equal names.
    //! @return false Compared configs are for events with non-equal names.
    bool name_eq(const event_config& r) const {
        return (event == r.event && custom_event_name == r.custom_event_name);
    }

    //! @brief Is-equal predicate based on event's caps.
    //!
    //! @param r
    //! @return true Compared configs have equal caps.
    //! @return false Compared configs have non-equal caps.
    bool caps_eq(const event_config& r) const {
        return (event == r.event && custom_event_name == r.custom_event_name);
    }

    std::string name() const {
        return event == proto::ET_CUSTOM ? custom_event_name
                                         : json(event).get<std::string>();
    }

private:
    //! \cond
    //! \internal nlohmann::json reflection mapper
    friend void to_json(json& j, const event_config& c) {
        if (c.event == ET_CUSTOM)
            j = {{"event", c.custom_event_name}};
        else
            j = {{"event", c.event}};

        j.update({{"active", c.active}});
        j.update({{"stream", c.stream}});
        j.update({{"snapshot", c.snapshot}});
        if (!__is_unset(c.period))
            j.update({{"period", c.period}});

        j["caps"] = c.caps;
    }

    //! \internal nlohmann::json reflection mapper
    friend void from_json(const json& j, event_config& c) {
        ignore_exception(j.at("event").get_to(c.event));
        if (c.event == ET_INVALID) {
            c.event = ET_CUSTOM;
            ignore_exception(j.at("event").get_to(c.custom_event_name));
        }

        ignore_exception(j.at("active").get_to(c.active));
        ignore_exception(j.at("stream").get_to(c.stream));
        ignore_exception(j.at("snapshot").get_to(c.snapshot));
        ignore_exception(j.at("period").get_to(c.period));
        //! caps should be ignorred if it's not getCamVideoConf response
        ignore_exception(j.at("caps").get_to(c.caps));
    }
    //! \endcond
};

//!
//! \brief Events config, list of event_config objects.
//!
//!
struct events_config {
    //! enabled:  bool, indicates global events and event-driven streaming
    //! enabling flag
    bool enabled {false};

    //! events: list of event_config struct
    std::vector<event_config> events;

    //!
    //! \brief Finds event which corresponds to event_config arg in the
    //!        events_config structure.
    //!
    //! @param[in] event - event_object, event_object.event used to find
    //!                    the event_config
    //! @param[out] result - if event_config found it will be storred here
    //! @return true event found
    //! @return false event not found
    bool get_event_config(const event_object& event, event_config& result) {
        for (auto eventConfig : this->events) {
            // For custom events we compare custom_event_name fields
            if (event.event == ET_CUSTOM) {
                if (eventConfig.custom_event_name == event.custom_event_name) {
                    result = eventConfig;
                    goto found;
                }
            }
            // Predefined events compared by type
            else if (eventConfig.event == event.event) {
                result = eventConfig;
                goto found;
            }
        }

        return false;
    found:
        return true;
    }

private:
    //! \internal nlohmann::json reflection mapper
    JSON_DEFINE_TYPE_INTRUSIVE(events_config, enabled, events);
};

//!
//! \brief Audio config
//!
//!
//! \internal 3.9 get_cam_audio_conf (SRV)
//! \internal 3.10 cam_audio_conf (CM)
//! \internal 3.11 set_cam_audio_conf (SRV)
struct audio_config {
    //! mic_gain: optional int range 0-100, microphone gain
    int mic_gain {UnsetInt};
    //! mic_mute: optional bool, microphone mute
    alter_bool mic_mute {alter_bool::B_INVALID};
    //! spkr_vol: optional int range 0-100, speaker volume
    int spkr_vol {UnsetInt};
    //! spkr_mute: optional bool, speaker mute
    alter_bool spkr_mute {alter_bool::B_INVALID};
    //! echo_cancel: optional string, echo cancellation mode, “” means off
    mode echo_cancel {mode::M_INVALID};

    //! caps
    audio_caps caps;

private:
    //! \internal nlohmann::json reflection mapper
    JSON_DEFINE_TYPE_INTRUSIVE(audio_config,
                               mic_gain,
                               mic_mute,
                               spkr_vol,
                               spkr_mute,
                               echo_cancel,
                               caps);
};

//! \internal
//! 3.5 cam_ptz (SRV)
//! 3.29 get_ptz_conf (SRV)
//! 3.30 cam_ptz_conf (CM)
//! 3.31 cam_ptz_preset (SRV)
//! 3.32 cam_ptz_preset_created (CM)
//! \endinternal

//! \brief PTZ preset
//!
struct ptz_preset {
    //! token: string, an unique token of preset what is used for all operations
    //! with preset
    std::string token {UnsetString};
    //! name: string, user friendly name of preset
    std::string name {UnsetString};

    //! actions: list of strings, required preset action.
    //! Possible values: “create”, “delete”, “goto”, “update”
    ptz_preset_action action {PA_INVALID};

private:
    //! \internal nlohmann::json reflection mapper.
    JSON_DEFINE_TYPE_INTRUSIVE(ptz_preset, token, name, action);
};

//! \brief PTZ config
//!
struct ptz_config {
    //! actions: list of strings, list of supported PTZ actions.
    //! Possible values: “left”, “right”, “top”, “bottom”, “zoom_in”,
    //! “zoom_out”, “stop”. Server sends commands via 3.5 cam_ptz (SRV)
    std::vector<ptz_action> actions;

    //! maximum_number_of_presets: optional int, max number of supported presets
    //! when camera supports. Zero value, the missed parameter or missed or
    //! empty presets list are interpreted by server as “camera doesn't support
    //! PTZ”
    int maximum_number_of_presets {UnsetInt};

    //! presets: optional list of structures ptz_preset
    std::vector<ptz_preset> presets;

private:
    //! \internal nlohmann::json reflection mapper
    JSON_DEFINE_TYPE_INTRUSIVE(ptz_config,
                               actions,
                               maximum_number_of_presets,
                               presets);
};

//! \brief PTZ command
//!
struct ptz_command {
    //! action: string,
    //! Camera informs server about list of supported actions with 3.30
    //! cam_ptz_conf (CM) command
    ptz_action action {A_INVALID};

    //! tm: optional int, operation time that allows to make PTZ with specified
    //! steps, msec
    int tm {UnsetInt};

private:
    //! \internal nlohmann::json reflection mapper.
    JSON_DEFINE_TYPE_INTRUSIVE(ptz_command, action, tm);
};

//!
//! \brief OSD config
//!
//! On Screen Display configuration object.
//! \internal 3.34 get_osd_conf (SRV)
//! \internal 3.35 osd_conf (CM)
//! \internal 3.36 set_osd_conf (SRV)
struct osd_config {
    //! system_id: optional bool, enable/disable static part of OSD
    //!
    bool system_id {false};
    //! system_id_text: optional string, a static content of OSD
    //!
    std::string system_id_text {UnsetString};
    //! time: optional bool, enable/disable time part of OSD
    //!
    bool time {false};
    //! time_format: optional string, one of predefined values from
    //! the time_format_n, should be included in caps.
    time_format_n time_format {TF_INVALID};
    //! date: optional bool, enable/disable date part of OSD
    //!
    bool date {false};
    //! date_format: optional string, one of predefined values from caps
    //!
    std::string date_format {UnsetString};
    //! font_size: optional string, one of predefined font sizes from caps
    //!
    std::string font_size {UnsetString};
    //! font_color: optional string, name of one of predefined font colors from
    //! caps
    std::string font_color {UnsetString};
    //! bkg_color: optional string, name of one of predefined background colors
    //! from caps
    std::string bkg_color {UnsetString};
    //! bkg_transp: optional bool, enable/disable OSD background transparency
    bool bkg_transp {false};
    //! alignment: optional string, one of predefined positions from caps
    std::string alignment {UnsetString};
    //! OSD capabilities of the device.
    //!
    osd_caps caps;

private:
    //! \internal nlohmann::json reflection mapper.
    JSON_DEFINE_TYPE_INTRUSIVE(osd_config,
                               system_id,
                               system_id_text,
                               time,
                               time_format,
                               date,
                               date_format,
                               font_size,
                               font_color,
                               bkg_color,
                               bkg_transp,
                               alignment,
                               caps);
};

//! \brief VXG Cloud access token
//!
struct access_token {
    typedef std::shared_ptr<access_token> ptr;
    //!
    //! @brief Socks proxy settings
    //!
    struct proxy_config {
        //! SOCKS4 proxy uri
        std::string socks4 {UnsetString};
        //! SOCKS5 proxy uri, ex. socks5://user:pwd\@host:port
        std::string socks5 {UnsetString};

    private:
        JSON_DEFINE_TYPE_INTRUSIVE(proxy_config, socks4, socks5);
    };

    //! \cond
    std::string token {UnsetString};
    int camid {UnsetInt};
    int cmngrid {UnsetInt};
    std::string access {UnsetString};
    std::string api {UnsetString};
    int api_p {UnsetInt};
    int api_sp {UnsetInt};
    std::string cam {UnsetString};
    int cam_p {UnsetInt};
    int cam_sp {UnsetInt};
    std::string svcp {UnsetString};

    proxy_config proxy;

    std::string _packed_ {UnsetString};
    //! \endcond

private:
    //! \internal nlohmann::json reflection mapper.
    JSON_DEFINE_TYPE_INTRUSIVE(access_token,
                               token,
                               camid,
                               cmngrid,
                               access,
                               api_p,
                               api_sp,
                               cam,
                               cam_p,
                               cam_sp,
                               svcp,
                               api,
                               proxy);

public:
    std::string api_uri(bool secure = true) {
        std::string proto = secure ? "https" : "http";
        int port = secure ? api_sp : api_p;

        return utils::string_format("%s://%s:%d", proto.c_str(), api.c_str(),
                                    port);
    };

    std::string pack() { return base64_encode(json(*this).dump()); }

    static access_token::ptr parse(std::string packed_token) {
        access_token::ptr result = nullptr;
        try {
            std::string decoded_token = base64_decode(packed_token);
            result = std::make_shared<access_token>(json::parse(decoded_token));
        } catch (const char* str) {
            vxg::logger::error("Unable to decode base64 token");
        } catch (const std::exception& e) {
            vxg::logger::error("Unable to parse access token");
        }
        return result;
    }
};
//! \endcond

//!
//! \brief Supported stream config
//!
struct supported_stream_config {
    //! id: string, name of media stream, unique for the camera
    std::string id {UnsetString};
    //! video: optional string, video ES that is sent in this media stream
    std::string video {UnsetString};
    //! audio: optional string, audio ES that is sent in this media stream
    std::string audio {UnsetString};

private:
    //! \internal nlohmann::json reflection mapper.
    JSON_DEFINE_TYPE_INTRUSIVE(supported_stream_config, id, video, audio);
};

//!
//! \brief Supported streams config, list of supported_stream_config.
//!
struct supported_streams_config {
    //! streams: list of supported_stream_config struct, camera media streams
    std::vector<supported_stream_config> streams;
    //! list of string, camera video ES
    std::vector<std::string> video_es;
    //! list of string, camera audio ES
    std::vector<std::string> audio_es;

private:
    //! \cond
    //! \internal nlohmann::json reflection mapper.
    JSON_DEFINE_TYPE_INTRUSIVE(supported_streams_config,
                               streams,
                               video_es,
                               audio_es);
    //! \endcond
};

//! \cond
bool operator==(const supported_streams_config a,
                const supported_streams_config b);
//! \endcond

//! \cond
//! \internal
//!
//! \brief Stream by event caps
//!
struct stream_by_event_config {
    //!
    //! \brief stream_by_event capabilities
    //!
    //!
    struct stream_by_event_caps {
        //! int, max duration of pre-event recording,  msec
        int pre_event_max {UnsetInt};
        //!  int, max duration of post-event recording, msec
        int post_event_max {UnsetInt};

    private:
        //! \internal nlohmann::json reflection mapper.
        JSON_DEFINE_TYPE_INTRUSIVE(stream_by_event_caps,
                                   pre_event_max,
                                   post_event_max);
    };

    //! stream_id: string, camera media stream to use. CM should use it to
    //! organize recording of pre-event even when real recording is not started.
    //! It allows to have a prepared pre-event buffer and be ready to meet and
    //! process an event even in the beginning of event-driven recording.
    std::string stream_id {UnsetString};
    //! pre_event: optional int, duration of stream before event, msec. CM must
    //! ensure at least pre_event milliseconds of recorded video for an event.
    //! Zero value means – pre-event recording is unnecessary
    int64_t pre_event {UnsetInt64};
    //! post_event: int, duration of stream after event, msec. CM must ensure at
    //! least post_event milliseconds of recorded video for an event. Zero value
    //! means – post-event recording is unnecessary
    int64_t post_event {UnsetInt64};

    //! stream_by_event_caps
    stream_by_event_caps caps;

private:
    //! \internal nlohmann::json reflection mapper.
    JSON_DEFINE_TYPE_INTRUSIVE(stream_by_event_config,
                               stream_id,
                               pre_event,
                               post_event,
                               caps);
};
//! \endcond

//! 5.6 audio_detection_config (CM)
//! Current audio detection settings. Reply 5.4 get_audio_detection (SRV).
struct audio_detection_config {
    //! level: int, audio volume in -dB
    int level {UnsetInt};
    //! length: int, duration before event trigger, msec
    int length {UnsetInt};

    struct audio_detection_conf_caps {
        //! level: (min:int, max:int, step:int), volume range and step in -dB
        std::tuple<int, int, int> level;

        JSON_DEFINE_TYPE_INTRUSIVE(audio_detection_conf_caps, level);
    };
    //! caps:
    audio_detection_conf_caps caps;

    JSON_DEFINE_TYPE_INTRUSIVE(audio_detection_config, level, length, caps);
};
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif