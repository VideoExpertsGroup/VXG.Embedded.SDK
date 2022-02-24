#pragma once

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 3.38 get_cam_memorycard_timeline (SRV)
//! Requests description of available memory card storage data.
struct get_cam_memorycard_timeline : public base_command {
    typedef std::shared_ptr<get_cam_memorycard_timeline> ptr;

    get_cam_memorycard_timeline() { cmd = GET_CAM_MEMORYCARD_TIMELINE; }
    virtual ~get_cam_memorycard_timeline() {}

    //! string, identifies the request. Must be included in response
    std::string request_id {UnsetString};
    //! string, start time of interval of interest. UTC time in the ISO8601
    //! format (YYYYMMDDThhmmss.mmm)
    std::string start {UnsetString};
    //! string, end time of interval of interest. UTC time in the ISO8601
    //! format (YYYYMMDDThhmmss.mmm)
    std::string end {UnsetString};

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(get_cam_memorycard_timeline,
                                       base_command,
                                       request_id,
                                       start,
                                       end);
};

enum memorycard_sync_status {
    MCSS_INVALID = -1,
    MCSS_DONE = 0,
    MCSS_PENDING,
    MCSS_ERROR,
    MCSS_CANCELED,
    MCSS_TOO_MANY_REQUESTS
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( memorycard_sync_status, {
    {MCSS_INVALID, nullptr},
    {MCSS_DONE, "done"},
    {MCSS_PENDING, "pending"},
    {MCSS_ERROR, "error"},
    {MCSS_CANCELED, "canceled"},
    {MCSS_TOO_MANY_REQUESTS, "too_many_requests"},
});
// clang-format on

struct memorycard_timeline_data {
    //! string, start time of a data segment. UTC time in the ISO8601 format
    //! (YYYYMMDDThhmmss.mmm)
    std::string start {UnsetString};
    //! string, start time of a data segment. UTC time in the ISO8601 format
    //! (YYYYMMDDThhmmss.mmm)
    std::string end {UnsetString};

    JSON_DEFINE_TYPE_INTRUSIVE(memorycard_timeline_data, start, end);
};

//! 3.39 cam_memorycard_timeline (CM)
//! Reply to 3.38
struct cam_memorycard_timeline : public base_command {
    typedef std::shared_ptr<cam_memorycard_timeline> ptr;

    cam_memorycard_timeline() { cmd = CAM_MEMORYCARD_TIMELINE; }
    virtual ~cam_memorycard_timeline() {}

    //! string, identifies the request. From get_cam_memorycard_timeline.
    std::string request_id {UnsetString};
    //! list of struct, describes available continuous data chunks
    std::vector<memorycard_timeline_data> data;
    //! string, start time of a data segment. UTC time in the ISO8601 format
    //! (YYYYMMDDThhmmss.mmm)
    std::string start {UnsetString};
    //! string, start time of a data segment. UTC time in the ISO8601 format
    //! (YYYYMMDDThhmmss.mmm)
    std::string end {UnsetString};

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(cam_memorycard_timeline,
                                       base_command,
                                       request_id,
                                       data,
                                       start,
                                       end);
};

//! 3.40 cam_memorycard_synchronize (SRV)
//! Request to download a certain fragment of data from camera storage to server
struct cam_memorycard_synchronize : public base_command {
    typedef std::shared_ptr<cam_memorycard_synchronize> ptr;

    cam_memorycard_synchronize() { cmd = CAM_MEMORYCARD_SYNCHRONIZE; }
    virtual ~cam_memorycard_synchronize() {}

    //! string, identifies the request. From get_cam_memorycard_timeline.
    std::string request_id {UnsetString};
    //! string, start time of a data segment. UTC time in the ISO8601 format
    //! (YYYYMMDDThhmmss.mmm)
    std::string start {UnsetString};
    //! string, start time of a data segment. UTC time in the ISO8601 format
    //! (YYYYMMDDThhmmss.mmm)
    std::string end {UnsetString};
    //! bool, True – camera should try to upload only missed data around
    //! existing (if they are) when possible. If the camera does not support
    //! this feature, it can ignore this server wish. False – all data in
    //! requested range may be uploaded freely.
    alter_bool exclude_overlap {alter_bool::B_INVALID};
    //! optional list of strings. List of request IDs. The presence of this list
    //! allows to create a new synchronization session and cancel the previous
    //! ones that are no longer relevant in one call.
    std::vector<std::string> cancel_requests;

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(cam_memorycard_synchronize,
                                       base_command,
                                       request_id,
                                       start,
                                       end,
                                       exclude_overlap,
                                       cancel_requests);
};

//! 3.41 cam_memorycard_synchronize_status (CM)
//! Request to download a certain fragment of data from camera storage to server
struct cam_memorycard_synchronize_status : public base_command {
    typedef std::shared_ptr<cam_memorycard_synchronize_status> ptr;

    cam_memorycard_synchronize_status() {
        cmd = CAM_MEMORYCARD_SYNCHRONIZE_STATUS;
    }
    virtual ~cam_memorycard_synchronize_status() {}

    //! string, identifies the request. From get_cam_memorycard_timeline.
    std::string request_id {UnsetString};
    //! progress: optional int, value from range 0-100 (%)
    int progress {UnsetInt};
    //! string. Possible values: “done”, “pending”, “error”, “canceled”,
    //! “too_many_requests”. “done” and, “error”, “too_many_requests” and
    //! “canceled” values assume operation completion or impossibility.
    //! “pending” together with progress field are used to update current
    //! progress
    memorycard_sync_status status {MCSS_INVALID};

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(cam_memorycard_synchronize_status,
                                       base_command,
                                       request_id,
                                       progress,
                                       status);
};

//! 3.42 cam_memorycard_synchronize_cancel (CM)
//! The client informs that the synchronization request is no longer valid and
//! should be canceled.
struct cam_memorycard_synchronize_cancel : public base_command {
    typedef std::shared_ptr<cam_memorycard_synchronize_cancel> ptr;

    cam_memorycard_synchronize_cancel() {
        cmd = CAM_MEMORYCARD_SYNCHRONIZE_CANCEL;
    }
    virtual ~cam_memorycard_synchronize_cancel() {}

    //! string, identifies the request. From get_cam_memorycard_timeline.
    std::string request_id {UnsetString};

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(cam_memorycard_synchronize_cancel,
                                       base_command,
                                       request_id);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
