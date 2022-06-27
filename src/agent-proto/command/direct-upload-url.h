#ifndef __COMMAND_DIRECT_UPLOAD_URL_H
#define __COMMAND_DIRECT_UPLOAD_URL_H

#include <agent-proto/command/command-list.h>
#include <agent-proto/command/get-direct-upload-url.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

struct direct_upload_url_base {
    typedef std::shared_ptr<direct_upload_url_base> ptr;
    //! category: string, must be "record", "snapshot" or "file_meta"
    upload_category category {UC_INVALID};
    //! status, TODO: convert to enum
    std::string status {UnsetString};
    //! optional string, the URL that should be used for upload
    std::string url {UnsetString};
    //! optional string, UTC time in the ISO8601 format YYYYMMDDThhmmss.mmm.
    //! Expiration time of the URL
    std::string expire {UnsetString};
    //! headers: optional struct, contains the set of mandatory HTTP-headers and
    //! their values (all as strings) that are required for successful
    //! uploading. Usually includes "Content-type" and "Content-length" headers
    std::map<std::string, std::string> headers;

    //! @internal
    //! Callback which should be called when upload finished.
    std::function<void(bool ok)> on_finished {[](bool ok) {}};
    //! Callback returns true if upload canceled.
    std::function<bool(void)> is_canceled {[]() { return false; }};
    //! TODO:
    std::shared_ptr<void> payload_data;
    //! @endinternal

    JSON_DEFINE_TYPE_INTRUSIVE(direct_upload_url_base,
                               category,
                               status,
                               url,
                               expire,
                               headers);
};

//! 3.23 direct_upload_url (SRV)
//! This is a response to 3.22 get_direct_upload_url (CM) or to 6.5 cam_event
//! (CM). Returns an URL for direct upload of a file to Cloud's storage
struct direct_upload_url : public base_command, public direct_upload_url_base {
    direct_upload_url() { cmd = DIRECT_UPLOAD_URL; }
    virtual ~direct_upload_url() {}

    //! event_id:
    int event_id {UnsetInt};
    //! extra: list of direct upload base object
    std::vector<direct_upload_url_base> extra;

    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE(direct_upload_url,
                                        base_command,
                                        direct_upload_url_base,
                                        extra);
};
}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
