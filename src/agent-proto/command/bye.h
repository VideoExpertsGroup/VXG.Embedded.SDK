#ifndef __COMMAND_BYE_H
#define __COMMAND_BYE_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! @brief Bye command reason.
enum bye_reason {
    //! ERROR – general application error
    BR_ERROR,
    //! SYSTEM_ERROR – system failure on peer
    BR_SYSTEM_ERROR,
    //! INVALID_USER – user not found
    BR_INVALID_USER,
    //! AUTH_FAILURE – authentication failure
    BR_AUTH_FAILURE,
    //! CONN_CONFLICT – there is another alive connection from the CM
    BR_CONN_CONFLICT,
    //! RECONNECT – no error but reconnection is required
    BR_RECONNECT,
    //! SHUTDOWN – CM shutdown or reboot is requested
    BR_SHUTDOWN,
    //! DELETED – CM has been deleted from account it belonged to. CM must stop
    //! all attempts to connect to server and forget all related data: account
    //! (user), password, server address and port.
    BR_DELETED,
    //! CONN_CLOSE - CM lost connection to the Cloud. May happen due to bad
    //! internet connection.
    BR_CONN_CLOSE,

    BR_INVALID = -1,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( bye_reason, {
    {BR_ERROR, "ERROR"},
    {BR_SYSTEM_ERROR, "SYSTEM_ERROR"},
    {BR_INVALID_USER, "INVALID_USER"},
    {BR_AUTH_FAILURE, "AUTH_FAILURE"},
    {BR_CONN_CONFLICT, "CONN_CONFLICT"},
    {BR_RECONNECT, "RECONNECT"},
    {BR_SHUTDOWN, "SHUTDOWN"},
    {BR_DELETED, "DELETED"},
    {BR_CONN_CLOSE, "CONN_CLOSE"},

    {BR_INVALID, nullptr},
})
// clang-format on

struct bye : public base_command {
    typedef std::shared_ptr<bye> ptr;

    bye() { cmd = BYE; }

    virtual ~bye() {}

    bye_reason reason {bye_reason::BR_INVALID};
    int retry {UnsetInt};

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(bye, base_command, reason, retry);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
