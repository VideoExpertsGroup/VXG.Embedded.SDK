#ifndef __COMMAND_GET_CAM_STATUS_H
#define __COMMAND_GET_CAM_STATUS_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 3.4 cam_status (CM)
//! Send camera current status. Reply 3.3 get_cam_status (SRV).
struct cam_status : public base_command {
    typedef std::shared_ptr<cam_status> ptr;
    cam_status() { cmd = CAM_STATUS; }

    virtual ~cam_status() {}

    //! ip: string, camera ip
    std::string ip {UnsetString};
    //! activity: bool, camera operational mode (see 2.7 cam_hello (SRV) for
    //! description).
    bool activity {false};
    //! streaming: bool, stream to any client is started
    bool streaming {false};
    //! status_led: bool, status led is on or off
    bool status_led {false};

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(cam_status,
                                       base_command,
                                       ip,
                                       activity,
                                       streaming,
                                       status_led);
};

//! 3.3 get_cam_status (SRV)
//! Request camera status.
struct get_cam_status : public base_command {
    typedef std::shared_ptr<get_cam_status> ptr;

    get_cam_status() { cmd = GET_CAM_STATUS; }

    virtual ~get_cam_status() {}

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE_NO_ARGS(get_cam_status, base_command);
};
}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
