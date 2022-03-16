#ifndef __CAPS_H
#define __CAPS_H

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <agent-proto/command/unset-helper.h>
#include <agent-proto/command/utils.h>

using json = nlohmann::json;

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {

#ifndef ignore_exception
#define ignore_exception(...) \
    try {                     \
        __VA_ARGS__;          \
    } catch (...) {           \
    }
#endif

//!
//! @brief Mode on/off
//!
enum mode {
    M_OFF,
    M_ON,
    M_AUTO,
    M_INVALID = -1,
};

//! \cond
// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( mode, {
    {M_INVALID, nullptr},
    {M_OFF, "off"},
    {M_ON, "on"},
    {M_AUTO, "auto"},
})
// clang-format on
//! \endcond

//! \cond
enum stream_reason {
    SR_RECORD,
    SR_RECORD_BY_EVENT,
    SR_SERVER_BY_EVENT,
    SR_LIVE,
    SR_INVALID = -1,
};
//! \endcond

//! \cond
// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( stream_reason, {
    {SR_INVALID, nullptr},
    //! “record” - purpose of streaming is continuous recording;
    {SR_RECORD, "record"},
    //! “record_by_event” - purpose of streaming is event-driven recording.
    //!  In this case CM starts streams, uploads pre-event files according to
    //!  rules of event-driven recording. “record” and “record_by_event” reasons
    //!  are mutually exclusive reasons. i.e. the same stream can't be started
    //!  for “record” and “record_by_event” reasons simultaneously.
    {SR_RECORD_BY_EVENT, "record_by_event"},
    //! TODO: add description
    {SR_RECORD_BY_EVENT, "server_by_event"},
    //! “live” - the purpose of streaming is monitoring
    {SR_LIVE, "live"},
})
// clang-format on
//! \endcond

//!
//! @brief Video codec format
//!
enum video_format {
    //! H264 (AVC)
    VF_H264,
    //! H265 (HEVC)
    VF_H265,
    //! Motion JPEG
    VF_MJPEG,
    //! Invalid value
    VF_INVALID = -1,
};

//! \cond
// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( video_format, {
    {VF_INVALID, nullptr},
    {VF_H264, "H.264"},
    {VF_H265, "H.265"},
    {VF_MJPEG, "MJPEG"},
})
// clang-format on
//! \endcond

//!
//! @brief Audio codec format
//!
enum audio_format {
    //! G711A - PCMA, A-Law
    AF_G711A,
    //! G711U - PCMU, U-Law
    AF_G711U,
    //! PCM
    AF_RAW,
    //! G726LE
    AF_ADPCM,
    AF_MP3,
    AF_NELLY8,
    AF_NELLY16,
    AF_NELLY,
    AF_OPUS,
    //! AAC
    AF_AAC,
    AF_SPEEX,
    //! Invalid value
    AF_INVALID = -1,
};

//! \cond
// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( audio_format, {
    {AF_INVALID, nullptr},
    {AF_RAW, "RAW"},
    {AF_ADPCM, "ADPCM"},
    {AF_MP3, "MP3"},
    {AF_NELLY8, "NELLY8"},
    {AF_NELLY16, "NELLY16"},
    {AF_NELLY, "NELLY"},
    {AF_SPEEX, "SPEEX"},
    {AF_OPUS, "OPUS"},
    {AF_G711U, "G711U"},
    {AF_G711A, "G711A"},
    {AF_AAC, "AAC"},
})
// clang-format on
//! \endcond

//! @brief Audio file format
enum audio_file_format {
    //! AU file format, encoded in mu-law and sampled with 8 or 16 kHz;
    AFF_AU_G711U,
    //! MP3 file format, in mono or stereo with bitrate of 64 kbps to 320 kbps
    //! and sample rate of 8 to 48 kHz.
    AFF_MP3,
    //! WAV file format, encoded in PCM audio that depends on what the product
    //! supports. It may support encoded as 8 or 16-bit mono or stereo and
    //! sample rate of 8 to 48 kHz;
    AFF_WAV_PCM,
    //! Invalid value
    AFF_INVALID = -1,
};

//! \cond
// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( audio_file_format, {
    {AFF_INVALID, nullptr},
    {AFF_AU_G711U, "AU_G711U"},
    {AFF_MP3, "MP3"},
    {AFF_WAV_PCM, "WAV_PCM"},
})
// clang-format on
//! \endcond

//!
//! @brief Media stream capabilites
//!
struct stream_caps {
    //! Video streams capabilities
    struct caps_video_object {
        //! Mandatory:
        //! list of strings, video ES that are covered by this capability config
        std::vector<std::string> streams;

        //! Mandatory:
        //! list of string, supported video formats; currently only “H.264” is
        //! supported
        std::vector<video_format> formats;

        //! Optional:
        //! list of pairs [string (format), string (profile)], list of profiles
        //! for formats (when they have). Empty list means –  color selection is
        //! not supported. “format” - one of listed in "formats" names.
        //! “profile”
        //! - name of profile. Example: [["H.264", "Baseline"], ["H.264",
        //! "Main"], ["H.264", "High"]]
        std::vector<std::pair<video_format, std::string>> profiles;

        //! Mandatory:
        //! list of pairs [int (horz), int (vert)], - supported video
        //! resolutions
        std::vector<std::pair<int, int>> resolutions;

        //! Mandatory:
        //! list of float, supported framerates
        std::vector<double> fps;

        //! Mandatory:
        //! VBR is supported
        bool vbr {false};

        //! Optional:
        //! [min:int, max:int], range of quality for VBR
        std::pair<int, int> quality;

        //! Mandatory:
        //! gop: [min:int, max:int, step:int], range of gop sizes
        std::vector<int> gop;

        //! Mandatory:
        //! [min:int, max:int, step:int], range of bitrates, kbps
        std::vector<int> brt;

        //! Optional:
        //! [min:int, max:int, step:int], range of bitrates, kbps
        std::vector<int> vbr_brt;

        //! Optional:
        //! True when stream smoothing can be controlled
        bool smoothing {false};

    private:
        JSON_DEFINE_TYPE_INTRUSIVE(caps_video_object,
                                   streams,
                                   formats,
                                   profiles,
                                   resolutions,
                                   fps,
                                   vbr,
                                   quality,
                                   gop,
                                   brt,
                                   vbr_brt,
                                   smoothing);
    };

    //!
    //! @brief Audio streams capabilities
    //!
    struct caps_audio_object {
        //! Mandatory:
        //! list of strings, audio ES that are covered by this capability config
        std::vector<std::string> streams;

        //! Mandatory:
        //!  list of string, supported audio formats; currently only “AAC” and
        //!  "G711U" is supported
        std::vector<audio_format> formats;

        //! Mandatory:
        //! [min:int, max:int, step:int], range of bitrates, kbps
        std::vector<int> brt;

        //! Mandatory:
        //! list of float, supported samplerates
        std::vector<double> srt;

    private:
        JSON_DEFINE_TYPE_INTRUSIVE(caps_audio_object,
                                   streams,
                                   formats,
                                   brt,
                                   srt);
    };

    //! List of video streams capabilities
    std::vector<caps_video_object> caps_video;
    //! List of audio streams capabilities
    std::vector<caps_audio_object> caps_audio;

private:
    JSON_DEFINE_TYPE_INTRUSIVE(stream_caps, caps_video, caps_audio);
};

//! \internal
//! Motion detection related structs
//!
//! 5.1 get_motion_detection (SRV)
//! 5.2 set_motion_detection (SRV)
//! 5.3 motion_detection_conf (CM)
//! \endinternal

//! @brief Motion sensitivity
enum motion_sensitivity {
    //! Indicates if sensitivity can be set for region
    MS_REGION,
    //! Indicates if sensitivity can be only for the full frame
    MS_FRAME,
    //! Invalid value
    MS_INVALID = -1,
};

//! \cond
// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( motion_sensitivity, {
    {MS_INVALID, nullptr},
    {MS_FRAME, "frame"},
    {MS_REGION, "region"},
})
// clang-format on
//! \endcond

//!
//! @brief Motion region shape
//!
enum motion_region_shape {
    //! Rectangle
    MR_RECTANGLE,
    //! Any shape
    MR_ANY,
    //! Invalid
    MR_INVALID = -1,
};

//! \cond
// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( motion_region_shape, {
    {MR_INVALID, nullptr},
    {MR_RECTANGLE, "rect"},
    {MR_ANY, "any"},
})
// clang-format on
//! \endcond

//!
//! @brief Motion detection capabilities
//! camera capabilities that limit possible motion detection configuration
//!
struct motion_detection_caps {
    //! Mandatory:
    //! supported number of motion regions
    size_t max_regions {0};

    //! Mandatory:
    //! (“region”, “frame”), default “region”;
    //! indicates if sensitivity can be set for region or for whole frame only
    motion_sensitivity sensitivity {MS_INVALID};

    //! Mandatory:
    //! (“rect”, “any”), default “any”; specifies limitation of region shape
    motion_region_shape region_shape {MR_INVALID};

private:
    JSON_DEFINE_TYPE_INTRUSIVE(motion_detection_caps,
                               max_regions,
                               sensitivity,
                               region_shape);
};

//!
//! @brief Video image capabilities
//!
//! \internal
//!
//! 3.6 get_cam_video_conf (SRV)
//! 3.7 cam_video_conf (CM)
//! 3.8 set_cam_video_conf (SRV)
//! \endinternal
struct video_caps {
    //! vert_flip: list of string, supported vertical flip modes, possible
    //! values [“off”, “on”, “auto”]
    std::vector<mode> vert_flip;

    //! horz_flip: list of string, supported horizontal flip modes, possible
    //! values [“off”, “on”, “auto”]
    std::vector<mode> horz_flip;

    //! tdn: list of string, supported TDM modes, possible values [“day”,
    //! “night”, “auto”]
    std::vector<std::string> tdn;

    //! ir_light: list of string, supported IR light modes, possible values
    //! [“off”, “on”, “auto”]
    std::vector<mode> ir_light;

    //! brightness: bool, True when camera supports brightness control
    bool brightness {false};

    //! contrast: bool, True when camera supports contrast control
    bool contrast {false};

    //! saturation: bool, True when camera supports saturation control
    bool saturation {false};

    //! sharpness: bool, True when camera supports sharpness control
    bool sharpness {false};

    //! nr_type: list of string, supported noise reduce types.
    //! Empty list when camera doesn't support it. Example: ["off", "normal",
    //! "expert"]
    std::vector<std::string> nr_type;

    //! nr_level: bool, True when noise reduce filter assumes control of NR
    //! level
    bool nr_level {false};

    //! wb_type: list of string, supported white balance types.
    //! Empty list when camera doesn't support it. Example: ["auto", "3200K
    //! (Indor)", "4200K (Fluo)", "5600K (Outdoor)"]
    std::vector<std::string> wb_type;

    //! pwr_frequency: bool, True camera supports compensation of images
    //! flickering due to flashing of lamps in indoor environment
    bool pwr_frequency {false};

private:
    JSON_DEFINE_TYPE_INTRUSIVE(video_caps,
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
                               pwr_frequency);
};

//!
//! @brief Events capabilies
//!
//! \internal
//! 6.2 get_cam_events (SRV)
//! 6.3 set_cam_events (SRV)
//! 6.4 cam_events_conf (CM)
//! 6.5 cam_event (CM)
//! \endinternal
struct event_caps {
    //! stream: bool, event can generate stream start
    bool stream {false};

    //! snapshot: bool, event is sent with snapshot
    bool snapshot {false};

    //! periodic: optional bool, the event is a periodic event
    //! (camera generates and processes it using specified time interval)
    bool periodic {false};

    //! trigger: optional bool, the event can be triggered externally, using 6.7
    bool trigger {false};

    //! @internal
    //! Event statefullness flag, indicates if event can be active during time
    //! period.
    //!
    //! For every statefull event the user code should generate 2 events with
    //! event.active = true and event.active = false, event.time of such events
    //! will be treated as event_start_time and event_stop_time correspondingly,
    //! the library will be uploading clips for this event during the
    //! [event_start_time - pre_rec_time, event_stop_time + post_rec_time]
    //! period.
    //!
    //! Only one event should be generated by the user if this flag set to
    //! false, in this case the library will try to upload clip of [event_time -
    //! pre_rec_time, event_time + pre_rec_time]
    //!
    //! This is vxgcloudagent library's internal flag which is not presented
    //! in the Cloud API currently.
    bool statefull {false};
    //! @endinternal

private:
    JSON_DEFINE_TYPE_INTRUSIVE(event_caps, stream, snapshot, periodic, trigger);
};

//!
//! @brief Audio capabilities
//!
//! \internal
//! 3.9 get_cam_audio_conf (SRV)
//! 3.10 cam_audio_conf (CM)
//! 3.11 set_cam_audio_conf (SRV)
//! \endinternal
struct audio_caps {
    //! mic: bool, microphone is supported
    alter_bool mic {alter_bool::B_INVALID};
    //! spkr: bool, speaker is supported
    //!
    alter_bool spkr {alter_bool::B_INVALID};
    //! echo_cancel: list of string, echo cancellation modes, empty or absent
    //! means not supported
    std::vector<mode> echo_cancel;

    //! backward: bool, backward audio supported. Obsolete. Server will ignore
    //! it when backward_formats exists. If true and  backward_formats is
    //! missed, server will interpret supported formats list as [“UNKNOWN”]
    alter_bool backward {alter_bool::B_INVALID};

    //! backward_formats: list of audio_format, list of supported backward
    //! formats. Supported values: [“RAW”, “ADPCM”, “MP3”, “NELLY8”, “NELLY16”,
    //! “NELLY”, “G711A”, “G711U”, “AAC”, “SPEEX”, “UNKNOWN”]. Empty list or
    //! missing parameter – camera doesn't support back audio channel.
    std::vector<audio_format> backward_formats;

    //! audio_file_formats: list of string, list of supported formats of audio
    //! files.
    std::vector<audio_file_format> audio_file_formats;

private:
    JSON_DEFINE_TYPE_INTRUSIVE(audio_caps,
                               mic,
                               spkr,
                               echo_cancel,
                               backward,
                               backward_formats,
                               audio_file_formats);
};

//! \internal 3.29 get_ptz_conf (SRV)
//! \internal 3.30 cam_ptz_conf (CM)
//! \internal 3.31 cam_ptz_preset (SRV)
//! \internal 3.32 cam_ptz_preset_created (CM)

//! @brief PTZ actions
//!
//!
enum ptz_action {
    //! Go left
    A_LEFT,
    //! Go right
    A_RIGHT,
    //! Go tip
    A_TOP,
    //! Go bottom
    A_BOTTOM,
    //! Zoom in
    A_ZOOM_IN,
    //! Zoom out
    A_ZOOM_OUT,
    //! Stop current action
    A_STOP,
    //! Invalid value
    A_INVALID = -1,
};

//! \cond
// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( ptz_action, {
    {A_INVALID, nullptr},
    {A_LEFT, "left"},
    {A_RIGHT, "right"},
    {A_TOP, "top"},
    {A_BOTTOM, "bottom"},
    {A_ZOOM_IN, "zoom_in"},
    {A_ZOOM_OUT, "zoom_out"},
    {A_STOP, "stop"},
})
// clang-format on
//! \endcond

//! @brief PTZ preset action
//!
enum ptz_preset_action {
    PA_CREATE,
    PA_DELETE,
    PA_GOTO,
    PA_UPDATE,

    PA_INVALID = -1,
};

//! \cond
// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( ptz_preset_action, {
    {PA_INVALID, nullptr},
    {PA_CREATE, "create"},
    {PA_DELETE, "delete"},
    {PA_GOTO, "goto"},
    {PA_UPDATE, "update"},
})
// clang-format on
//! \endcond

//! 3.34 get_osd_conf (SRV)
//! 3.35 osd_conf (CM)
//! 3.36 set_osd_conf (SRV)

//!
//! @brief Time format
//!
//!
enum time_format_n {
    //! 12 hours
    TF_12H,
    //! 24 hours
    TF_24H,

    //! Invalid value
    TF_INVALID = -1,
};

//! \cond
// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( time_format_n, {
    {TF_INVALID, nullptr},
    {TF_12H, "12h"},
    {TF_24H, "24h"},
})
// clang-format on
//! \endcond

//!
//! @brief OSD capabilities
//!
struct osd_caps {
    //! system_id: bool, True when OSD supports separate system_id
    //! enabling/disabling
    bool system_id {false};
    //! system_id_text: bool, True when OSD supports separate system_id
    //! customization
    bool system_id_text {false};
    //! time: bool, True when OSD supports separate time enabling/disabling
    bool time {false};
    //! time_format: list of string, supported time formats.
    //! Empty list means – time format selection is not supported. Example:
    //! ["12h", "24h"]
    std::vector<time_format_n> time_format;
    //! date: bool, True when OSD supports separate date enabling/disabling
    bool date {false};
    //! date_format: list of string, supported date formats. Empty list means –
    //! date format selection is not supported. Example: ["YYYY-MM-DD",
    //! "MM-DD-YYYY", "DD-MM-YYYY", "YYYY/MM/DD", "MM/DD/YYYY2, "DD/MM/YYYY"]
    std::vector<std::string> date_format;
    //! font_size: list of string, describes supported font sizes.
    //! Empty list means – font size format selection is not supported.
    //! Examples: ["16", "32", "48", "64", "auto"] or ["Small", "Normal", "Big"]
    std::vector<std::string> font_size;
    //! font_color: list of pairs [string (name), optional string (value)],
    //! predefined set of possible font colors. Empty list means –  color
    //! selection is not supported. Optioanal value is a RGB color code in HEX.
    //! Example: [["Orange", "FF9C00"]]
    std::vector<std::pair<std::string, std::string>> font_color;
    //! bkg_color: list of pairs [string (name), optional string (value)],
    //! predefined set of possible background colors.
    //! Empty list means –  color selection is not supported.
    //! Optioanal value is a RGB color code in HEX. Example: [["Black",
    //! "000000"]]
    std::vector<std::pair<std::string, std::string>> bkg_color;
    //! bkg_transp: bool, True when OSD supports background transparency
    bool bkg_transp {false};
    //! alignment: list of strings, supported OSD positions.
    //! Empty list means –  position can't be changed.
    //! Example: ["UpperLeft", "UpperRight", "LowerLeft", "LowerRight"]
    std::vector<std::string> alignment;

private:
    JSON_DEFINE_TYPE_INTRUSIVE(osd_caps,
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
                               alignment);
};
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg

#endif  // __CAPS_H