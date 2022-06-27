#ifndef __COMMAND_GET_MOTION_DETECTION_H
#define __COMMAND_GET_MOTION_DETECTION_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 5.3 motion_detection_conf (CM)
//! Send description and current state of motion map. Reply 5.1
//! get_motion_detection (SRV).
struct motion_detection_conf : public base_command,
                               public proto::motion_detection_config {
    typedef std::shared_ptr<motion_detection_conf> ptr;

    motion_detection_conf() { cmd = MOTION_DETECTION_CONF; }

    virtual ~motion_detection_conf() {}

    //! XXX
    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(motion_detection_conf,
                                                base_command,
                                                proto::motion_detection_config);
};

//! 5.1 get_motion_detection (SRV)
//! Request for motion detection map.
//! Response: 5.3 motion_detection_conf (CM) or  3.1 done (CM) with
//! code=NOT_SUPPORTED, if motion detection is not supported

struct get_motion_detection : public base_command {
    typedef std::shared_ptr<get_motion_detection> ptr;

    get_motion_detection() { cmd = GET_MOTION_DETECTION; }

    virtual ~get_motion_detection() {}

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE_NO_ARGS(get_motion_detection,
                                               base_command);
};
}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
