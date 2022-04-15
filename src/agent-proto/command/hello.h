#ifndef __COMMAND_HELLO_H
#define __COMMAND_HELLO_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

enum hello_status {
    // OK
    HS_OK,

    HS_INVALID = -1,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( hello_status, {
    {HS_OK, "OK"},

    {HS_INVALID, nullptr},
})
// clang-format on

struct hello : public base_command {
    hello() { cmd = HELLO; }

    virtual ~hello() {}

    hello_status status {HS_INVALID};
    // optional string, CA certificate used for OpenVPN server identification
    std::string ca {UnsetString};

    // optional string, current session ID. When CM receives the value, it
    // should save it and reuse during next connection in 2.3 register (CM)
    std::string sid {UnsetString};

    // optional string, media data uploading URL. Depending on the type (secure
    // / not secure) of the CM's connection to the server, the server offers a
    // secure or not secure way of content  upload (CM uses POST requests for
    // pre-event-files and snapshots uploading). For detailed information see .
    std::string upload_uri {UnsetString};
    // deprecated
    std::string upload_url {UnsetString};

    std::string media_server {UnsetString};
    std::string connid {UnsetString};

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(hello,
                                       base_command,
                                       ca,
                                       sid,
                                       upload_uri,
                                       upload_url,
                                       media_server,
                                       connid);
};
}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
