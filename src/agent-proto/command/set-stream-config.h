#ifndef __COMMAND_SET_STREAM_CONFIG_H
#define __COMMAND_SET_STREAM_CONFIG_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

struct set_stream_config : public base_command, public proto::stream_config {
    set_stream_config() { cmd = SET_STREAM_CONFIG; }

    virtual ~set_stream_config() {}

    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(set_stream_config,
                                                base_command,
                                                proto::stream_config);
};
}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
