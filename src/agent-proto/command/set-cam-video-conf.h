#ifndef __COMMAND_SET_CAM_VIDEO_CONF_H
#define __COMMAND_SET_CAM_VIDEO_CONF_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 3.8 set_cam_video_conf (SRV)
//! Set video source parameters.
struct set_cam_video_conf : public base_command, public proto::video_config {
    set_cam_video_conf() { cmd = SET_CAM_VIDEO_CONF; }

    virtual ~set_cam_video_conf() {}

    //! XXX
    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(set_cam_video_conf,
                                                base_command,
                                                proto::video_config);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
