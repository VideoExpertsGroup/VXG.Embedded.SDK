#ifndef __COMMAND_CAM_PT_Z_H
#define __COMMAND_CAM_PT_Z_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 3.5 cam_ptz (SRV)
//! Send PTZ command to the camera.
struct cam_ptz : public base_command, public proto::ptz_command {
    typedef std::shared_ptr<cam_ptz> ptr;

    cam_ptz() { cmd = CAM_PTZ; }

    virtual ~cam_ptz() {}

    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(cam_ptz,
                                                base_command,
                                                proto::ptz_command);
};

//! 3.31 cam_ptz_preset (SRV)
//! Server sends the command to perform an operation with PTZ preset.
struct cam_ptz_preset : public base_command, public proto::ptz_preset {
    typedef std::shared_ptr<cam_ptz_preset> ptr;

    cam_ptz_preset() { cmd = CAM_PTZ_PRESET; }

    virtual ~cam_ptz_preset() {}

    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(cam_ptz_preset,
                                                base_command,
                                                proto::ptz_preset);
};

//! 3.32 cam_ptz_preset_created (CM)
//! Reply 3.31 cam_ptz_preset (SRV) “create” action.
struct cam_ptz_preset_created : public base_command {
    typedef std::shared_ptr<cam_ptz_preset_created> ptr;

    cam_ptz_preset_created() { cmd = CAM_PTZ_PRESET_CREATED; }

    virtual ~cam_ptz_preset_created() {}

    std::string token {UnsetString};

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(cam_ptz_preset_created,
                                       base_command,
                                       token);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg

#endif
