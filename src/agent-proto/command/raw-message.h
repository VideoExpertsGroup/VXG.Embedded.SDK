#ifndef __COMMAND_RAW_MESSAGE_H
#define __COMMAND_RAW_MESSAGE_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 3.26 raw_message (SRV)
//! A client sends a text message to a camera. Format of the messages is up to
//! client and camera.
struct raw_message : public base_command {
    typedef std::shared_ptr<raw_message> ptr;

    raw_message() { cmd = RAW_MESSAGE; }

    virtual ~raw_message() {}

    //! client_id: string, ID of a client what sends the message
    std::string client_id {UnsetString};
    //! message: string, content of the message.
    std::string message {UnsetString};

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(raw_message,
                                       base_command,
                                       client_id,
                                       message);
};

struct raw_message_client_connected : public base_command {
    typedef std::shared_ptr<raw_message_client_connected> ptr;

    raw_message_client_connected() { cmd = RAW_MESSAGE_CLIENT_CONNECTED; }

    virtual ~raw_message_client_connected() {}

    //! client_id: string, ID of a client what sends the message
    std::string client_id {UnsetString};

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(raw_message_client_connected,
                                       base_command,
                                       client_id);
};

struct raw_message_client_disconnected : public base_command {
    typedef std::shared_ptr<raw_message_client_disconnected> ptr;

    raw_message_client_disconnected() { cmd = RAW_MESSAGE_CLIENT_DISCONNECTED; }

    virtual ~raw_message_client_disconnected() {}

    //! client_id: string, ID of a client what sends the message
    std::string client_id {UnsetString};

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(raw_message_client_disconnected,
                                       base_command,
                                       client_id);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
