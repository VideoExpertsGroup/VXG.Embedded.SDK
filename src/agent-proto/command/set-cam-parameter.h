#ifndef __COMMAND_SET_CAM_PARAMETER_H
#define __COMMAND_SET_CAM_PARAMETER_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 3.12 set_cam_parameter (SRV)
//! Set various camera parameters. Most of them are reported in 3.4 cam_status
//! (CM).
struct set_cam_parameter : public base_command {
    set_cam_parameter() { cmd = SET_CAM_PARAMETER; }

    virtual ~set_cam_parameter() {}

    //! status_led: optional bool, camera status LED switch; should be ignored
    //! if not supported
    bool status_led {false};
    //! activity: optional bool, change camera operational mode if specified
    bool activity {false};

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(set_cam_parameter,
                                       base_command,
                                       status_led,
                                       activity);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
