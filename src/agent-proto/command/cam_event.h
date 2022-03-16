#ifndef __COMMAND_CAM_EVENT_H
#define __COMMAND_CAM_EVENT_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

struct cam_event : public base_command, public proto::event_object {
    typedef std::shared_ptr<cam_event> ptr;

    cam_event() { base_command::cmd = CAM_EVENT; }
    //! XXX
    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(cam_event,
                                                base_command,
                                                proto::event_object);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif