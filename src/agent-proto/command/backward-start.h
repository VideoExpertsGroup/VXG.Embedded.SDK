#ifndef __COMMAND_BACKWARD_START_H
#define __COMMAND_BACKWARD_START_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {
//! 4.142 backward_start (SRV)
//! CM should receive incoming backward-stream at specified URL.
struct backward_start : public base_command {
    backward_start() { cmd = BACKWARD_START; }

    virtual ~backward_start() {}

    //! url: string, URL to which client is going to publish the stream
    std::string url;

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(backward_start, base_command, url);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg

#endif
