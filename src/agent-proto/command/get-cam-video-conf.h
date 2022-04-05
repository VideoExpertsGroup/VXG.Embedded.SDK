#ifndef __COMMAND_GET_CAM_VIDEO_CONF_H
#define __COMMAND_GET_CAM_VIDEO_CONF_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 3.7 cam_video_conf (CM)
//! Video source parameters. Reply 3.6 get_cam_video_conf (SRV).
struct cam_video_conf : public base_command, public proto::video_config {
    typedef std::shared_ptr<cam_video_conf> ptr;

    cam_video_conf() { cmd = CAM_VIDEO_CONF; }

    virtual ~cam_video_conf() {}

    //! XXX
    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(cam_video_conf,
                                                base_command,
                                                proto::video_config);
};
//! 3.6 get_cam_video_conf (SRV)
//! Request video source parameters.
struct get_cam_video_conf : public base_command {
    typedef std::shared_ptr<get_cam_video_conf> ptr;

    get_cam_video_conf() { cmd = GET_CAM_VIDEO_CONF; }

    virtual ~get_cam_video_conf() {}

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE_NO_ARGS(get_cam_video_conf,
                                               base_command);
};
}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
