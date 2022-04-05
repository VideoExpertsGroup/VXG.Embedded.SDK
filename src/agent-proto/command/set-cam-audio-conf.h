#ifndef __COMMAND_SET_CAM_AUDIO_CONF_H
#define __COMMAND_SET_CAM_AUDIO_CONF_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 3.11 set_cam_audio_conf (SRV)
//! Set audio channel parameters.
struct set_cam_audio_conf : public base_command, public proto::audio_config {
    set_cam_audio_conf() { cmd = SET_CAM_AUDIO_CONF; }

    virtual ~set_cam_audio_conf() {}

    //! XXX
    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(set_cam_audio_conf,
                                                base_command,
                                                proto::audio_config);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
