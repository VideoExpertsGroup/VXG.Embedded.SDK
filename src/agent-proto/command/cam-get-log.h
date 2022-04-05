#ifndef __COMMAND_CAM_GET_LOG_H
#define __COMMAND_CAM_GET_LOG_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

struct cam_get_log : public base_command {
    cam_get_log() { cmd = CAM_GET_LOG; }

    virtual ~cam_get_log() {}

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE_NO_ARGS(cam_get_log, base_command);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
