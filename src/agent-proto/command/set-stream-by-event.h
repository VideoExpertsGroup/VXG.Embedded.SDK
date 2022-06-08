#ifndef __COMMAND_SET_STREAM_BY_EVENT_H
#define __COMMAND_SET_STREAM_BY_EVENT_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

struct set_stream_by_event : public base_command,
                             public proto::stream_by_event_config {
    set_stream_by_event() { cmd = SET_STREAM_BY_EVENT; }

    virtual ~set_stream_by_event() {}

    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(set_stream_by_event,
                                                base_command,
                                                stream_by_event_config);
};
}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
