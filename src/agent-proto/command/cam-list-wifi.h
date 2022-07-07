#ifndef __COMMAND_CAM_LIST_WI_FI_H
#define __COMMAND_CAM_LIST_WI_FI_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 3.20 cam_wifi_list (CM)
//! Lists Wi-Fi networks detected by camera
struct cam_wifi_list : public base_command, public proto::wifi_list {
    typedef std::shared_ptr<cam_wifi_list> ptr;

    cam_wifi_list() { cmd = CAM_WIFI_LIST; }

    virtual ~cam_wifi_list() {}
    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(cam_wifi_list,
                                                base_command,
                                                proto::wifi_list);
};

//! 3.19 cam_list_wifi (SRV)
//! Request enumeration of available Wi-Fi networks.
struct cam_list_wifi : public base_command {
    typedef std::shared_ptr<cam_list_wifi> ptr;

    cam_list_wifi() { cmd = CAM_LIST_WIFI; }

    virtual ~cam_list_wifi() {}

    //! XXX
    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE_NO_ARGS(cam_list_wifi, base_command);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
