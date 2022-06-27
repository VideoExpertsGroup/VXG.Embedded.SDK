#ifndef __COMMAND_CAM_REGISTER_H
#define __COMMAND_CAM_REGISTER_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 2.6 cam_register (CM)
//! Identify camera on the Server. This is the first command that initiates work
//! of the camera in CM ↔ Cloud interaction. CM must not send the command when
//! the camera is off. This means: when the command is received by the server –
//! camera is online.
struct cam_register : public base_command {
    typedef std::shared_ptr<cam_register> ptr;

    cam_register() { base_command::cmd = CAM_REGISTER; }

    virtual ~cam_register() {}

    //! ip: string, camera IP address
    std::string ip {UnsetString};
    //! uuid: string,  camera unique ID got from 2.5 configure (SRV)
    std::string uuid {UnsetString};
    //! brand: string, camera brand name
    std::string brand {UnsetString};
    //! model: string, camera model name
    std::string model {UnsetString};
    //! sn: string, serial number
    std::string sn {UnsetString};
    //! raw_messaging: optional bool, True – camera supports raw messages
    //! exchange, False (or the parameter is absent) – it doesn't.
    bool raw_messaging {false};
    //! version: string, firmware version
    std::string version {UnsetString};
    //! type: optional string, design of the device
    std::string type;

    //! p2p struct
    //! NIY

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(cam_register,
                                       base_command,
                                       ip,
                                       uuid,
                                       brand,
                                       model,
                                       sn,
                                       raw_messaging,
                                       version,
                                       type);
};
}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif