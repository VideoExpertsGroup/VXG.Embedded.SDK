#ifndef __COMMAND_SET_CAM_EVENTS_H
#define __COMMAND_SET_CAM_EVENTS_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

struct set_cam_events : public base_command, public proto::events_config {
    set_cam_events() { cmd = SET_CAM_EVENTS; }

    virtual ~set_cam_events() {}

    //! XXX
    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(set_cam_events,
                                                base_command,
                                                proto::events_config);
};
}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
