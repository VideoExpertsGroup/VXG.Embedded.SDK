#ifndef __COMMAND_BACKWARD_STOP_H
#define __COMMAND_BACKWARD_STOP_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 4.153 backward_stop (SRV)
//! CM should stop receiving of incoming stream.
struct backward_stop : public base_command {
    backward_stop() { cmd = BACKWARD_STOP; }

    virtual ~backward_stop() {}

    //! string, URL to which client published the stream
    std::string url {UnsetString};

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(backward_stop, base_command, url);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
