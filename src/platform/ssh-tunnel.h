#include "libssh2-tunnel.h"

#include <string>
#include <unordered_map>

namespace vxg {
namespace cloud {
namespace tunnel {
namespace ssh {

enum port_forward_cmd {
    PF_GET_KEY,
    PF_CREATE_TUNNEL,

    PF_INVALID = -1,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( port_forward_cmd, {
    {PF_GET_KEY, "get_key"},
    {PF_CREATE_TUNNEL, "create_tunnel"},

    {PF_INVALID, nullptr},
})
// clang-format on

enum port_forward_status {
    PFS_OK,
    PFS_ERROR,
    PFS_NOT_ALLOWED,

    PFS_INVALID = -1,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( port_forward_status, {
    {PFS_OK, "Ok"},
    {PFS_ERROR, "Error"},
    {PFS_NOT_ALLOWED, "Not allowed"},

    {PFS_INVALID, nullptr},
})
// clang-format on

struct port_forward_params {
    std::string cam_ip;
    int cam_port;
    int port_out;
    std::string user;
    std::string host;
    int timeout;

    JSON_DEFINE_TYPE_INTRUSIVE(port_forward_params,
                               cam_ip,
                               cam_port,
                               port_out,
                               user,
                               host,
                               timeout);
};

struct port_forward_message {
    port_forward_cmd cmd {PF_INVALID};
    int msgid {UnsetInt};
    int refid {UnsetInt};
    port_forward_status status {PFS_INVALID};
    std::string text {UnsetString};
    std::string key {UnsetString};
    json data;

    static int _msgid;

    port_forward_message() : msgid {_msgid++} {}

    JSON_DEFINE_TYPE_INTRUSIVE(port_forward_message,
                               cmd,
                               msgid,
                               refid,
                               status,
                               text,
                               key,
                               data);
};
int port_forward_message::_msgid = 1;

class tunneling {
    typedef int id;

    std::hash<std::string> hasher_;
    std::map<size_t, libssh2::reverse_tunnel::ptr> sessions;

    size_t hash_session(std::string ssh_user,
                        std::string ssh_server,
                        uint16_t ssh_port) {
        return hasher_(ssh_user + "$" + ssh_server + "$" +
                       std::to_string(ssh_port));
    }

public:
    static tunneling instance() {
        static tunneling t;
        return t;
    }

    id open_session(std::string local_host,
                    uint16_t local_port,
                    uint16_t remote_port,
                    std::string ssh_server,
                    uint16_t ssh_port,
                    std::string ssh_user,
                    std::string ssh_private_key,
                    std::string ssh_public_key) {
        auto session_id = hash_session(ssh_user, ssh_server, ssh_port);

        if (sessions[session_id] != nullptr) {
            spdlog::warn("Tunnel for key {} already opened");
            return session_id;
        } else {
            libssh2::reverse_tunnel::params p;
            // p.local_dest_ip = local_host;
            // p.local_dest_port = local_port;
            // p.remote_listen_port = remote_port;
            p.ssh_server_username = ssh_user;

            sessions[session_id]
        }
    }
};

}  // namespace ssh
}  // namespace tunnel
}  // namespace cloud
}  // namespace vxg