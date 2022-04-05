#ifndef __COMMAND_GET_PTZ_CONF_H
#define __COMMAND_GET_PTZ_CONF_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 3.30 cam_ptz_conf (CM)
//! Camera PTZ parameters. Reply 3.29 get_ptz_conf (SRV).
struct cam_ptz_conf : public base_command, public proto::ptz_config {
    typedef std::shared_ptr<cam_ptz_conf> ptr;

    cam_ptz_conf() { cmd = CAM_PTZ_CONF; }

    virtual ~cam_ptz_conf() {}

    //! XXX
    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(cam_ptz_conf,
                                                base_command,
                                                proto::ptz_config);
};

//! 3.29 get_ptz_conf (SRV)
//! Request PTZ configuration.
struct get_ptz_conf : public base_command {
    typedef std::shared_ptr<get_ptz_conf> ptr;

    get_ptz_conf() { cmd = GET_PTZ_CONF; }

    virtual ~get_ptz_conf() {}

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE_NO_ARGS(get_ptz_conf, base_command);
};
}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
