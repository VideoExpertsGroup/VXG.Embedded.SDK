#ifndef __COMMAND_GET_CAM_AUDIO_CONF_H
#define __COMMAND_GET_CAM_AUDIO_CONF_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 3.10 cam_audio_conf (CM)
//! Audio channel parameters. Reply 3.9 get_cam_audio_conf (SRV).
struct cam_audio_conf : public base_command, public proto::audio_config {
    typedef std::shared_ptr<cam_audio_conf> ptr;

    cam_audio_conf() { cmd = CAM_AUDIO_CONF; }

    virtual ~cam_audio_conf() {}

    //! XXX
    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(cam_audio_conf,
                                                base_command,
                                                proto::audio_config);
};

//! 3.9 get_cam_audio_conf (SRV)
//! Request audio channel parameters.
struct get_cam_audio_conf : public base_command {
    typedef std::shared_ptr<get_cam_audio_conf> ptr;

    get_cam_audio_conf() { cmd = GET_CAM_AUDIO_CONF; }

    virtual ~get_cam_audio_conf() {}

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE_NO_ARGS(get_cam_audio_conf,
                                               base_command);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
