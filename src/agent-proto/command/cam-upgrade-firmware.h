#ifndef __COMMAND_UPGRADE_FIRMWARE_H
#define __COMMAND_UPGRADE_FIRMWARE_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 3.16 cam_upgrade_firmware (SRV)
//! Upgrade camera firmware.
struct cam_upgrade_firmware : public base_command {
    typedef std::shared_ptr<cam_update_preview> ptr;

    cam_upgrade_firmware() { cmd = CAM_UPGRADE_FIRMWARE; }

    virtual ~cam_upgrade_firmware() {}

    std::string url;

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(cam_upgrade_firmware, base_command, url);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
