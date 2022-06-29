#ifndef __COMMAND_UPDATE_PREVIEW_H
#define __COMMAND_UPDATE_PREVIEW_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 3.15 cam_update_preview (SRV)
//! Request preview image update.
struct cam_update_preview : public base_command {
    typedef std::shared_ptr<cam_update_preview> ptr;

    cam_update_preview() { cmd = CAM_UPDATE_PREVIEW; }

    virtual ~cam_update_preview() {}

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE_NO_ARGS(cam_update_preview,
                                               base_command);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
