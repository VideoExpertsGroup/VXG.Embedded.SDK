#ifndef __COMMAND_GET_CAM_EVENTS_H
#define __COMMAND_GET_CAM_EVENTS_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

struct cam_events_conf : public base_command, public proto::events_config {
    typedef std::shared_ptr<cam_events_conf> ptr;

    cam_events_conf() { cmd = CAM_EVENTS_CONF; }

    virtual ~cam_events_conf() {}
    //! XXX
    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(cam_events_conf,
                                                base_command,
                                                proto::events_config);
};

struct get_cam_events : public base_command {
    typedef std::shared_ptr<get_cam_events> ptr;

    get_cam_events() { cmd = GET_CAM_EVENTS; }

    virtual ~get_cam_events() {}

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE_NO_ARGS(get_cam_events, base_command);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
