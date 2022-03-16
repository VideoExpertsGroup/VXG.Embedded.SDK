#ifndef __COMMAND_CAM_SET_CURRENT_WI_FI_H
#define __COMMAND_CAM_SET_CURRENT_WI_FI_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 3.21 cam_set_current_wifi (SRV)
//! Change currently used Wi-Fi by camera.
struct cam_set_current_wifi : public base_command, public proto::wifi_network {
    cam_set_current_wifi() { cmd = CAM_SET_CURRENT_WIFI; }

    virtual ~cam_set_current_wifi() {}

    //! XXX
    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(cam_set_current_wifi,
                                                base_command,
                                                proto::wifi_network);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
