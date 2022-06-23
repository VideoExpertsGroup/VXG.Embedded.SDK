#ifndef __COMMAND_SET_MOTION_DETECTION_H
#define __COMMAND_SET_MOTION_DETECTION_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

struct set_motion_detection : public base_command,
                              public proto::motion_detection_config {
    set_motion_detection() { cmd = SET_MOTION_DETECTION; }

    virtual ~set_motion_detection() {}

    //! XXX
    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(set_motion_detection,
                                                base_command,
                                                proto::motion_detection_config);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
