#ifndef __COMMAND_GET_DIRECT_UPLOAD_URL_H
#define __COMMAND_GET_DIRECT_UPLOAD_URL_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

// clang-format off
enum upload_category {
    UC_RECORD,
    UC_SNAPSHOT,
    UC_FILE_META,

    UC_INVALID = -1,
};

NLOHMANN_JSON_SERIALIZE_ENUM( upload_category, {
    {UC_RECORD, "record"},
    {UC_SNAPSHOT, "snapshot"},
    {UC_FILE_META, "file_meta"},

    {UC_INVALID, nullptr},
})

enum media_type {
    MT_MP4,
    MT_JPEG,

    MT_INVALID = -1,
};

NLOHMANN_JSON_SERIALIZE_ENUM( media_type, {
    {MT_MP4, "mp4"},
    {MT_JPEG, "jpg"},

    {MT_INVALID, nullptr},
})
// clang-format on

//! 3.22 get_direct_upload_url (CM)
//! Request for direct upload of a file to Cloud's storage.
struct get_direct_upload_url : public base_command {
    typedef std::shared_ptr<get_direct_upload_url> ptr;

    get_direct_upload_url() { cmd = GET_DIRECT_UPLOAD_URL; }

    virtual ~get_direct_upload_url() {}

    //! category: string, must be "record" or “snapshot”
    upload_category category {UC_INVALID};
    //! type: string, media file type; must be "mp4" for "record" or “jpg” for
    //! “snapshot”
    media_type type {MT_INVALID};
    //! stream_id: optional string, media stream ID. Used with "record". Please
    //! see 4.8 set_stream_by_event (SRV)
    std::string stream_id {UnsetString};
    //! file_time: string, UTC time in the ISO8601 format YYYYMMDDThhmmss.mmm.
    //! Record start time or time-stamp of a snapshot that must match with
    //! “image_time” of event's “snapshot_info”
    std::string file_time {UnsetString};
    //! duration: optional int, only for "record". Duration of fragment in msec
    int duration {UnsetInt};
    //! size: int, size of uploaded file in bytes
    int size {UnsetInt};
    //! width: optional int, only for “snapshot”. Width of image in pixels
    int width {UnsetInt};
    //! height: optional int, only for “snapshot”. Height of image in pixels
    int height {UnsetInt};

    //! @internal
    //! Callback which should be called when upload finished.
    std::function<void(bool ok)> on_finished {[](bool ok) {}};
    //! Callback returns true if upload canceled.
    std::function<bool(void)> is_canceled {[]() { return false; }};
    //! Upload ticket, requestid from memorycard_sync request, if empty this
    //! is not memorycard sync requested upload but event triggered
    std::string memorycard_sync_ticket {""};
    //! Duration of fragment in microseconds, used to accurate calculations,
    //! as milliseconds @ref get_direct_upload_url.duration.
    vxg::cloud::duration duration_us {cloud::duration(0)};
    //! @endinternal

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(get_direct_upload_url,
                                       base_command,
                                       category,
                                       type,
                                       stream_id,
                                       file_time,
                                       duration,
                                       size,
                                       width,
                                       height);

    bool operator<(vxg::cloud::agent::proto::command::get_direct_upload_url r) {
        return vxg::cloud::utils::time::from_iso_packed(file_time) <
               vxg::cloud::utils::time::from_iso_packed(r.file_time);
    }
};
}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
