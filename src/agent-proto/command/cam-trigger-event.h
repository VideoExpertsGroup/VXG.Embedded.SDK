#ifndef __COMMAND_CAM_TRIGGER_EVENT_H
#define __COMMAND_CAM_TRIGGER_EVENT_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 6.7 cam_trigger_event (SRV)
//! Request CM to generate and send event to the server. This is helpful when
//! event is externally triggered by another system using API or for debugging.
struct cam_trigger_event : public base_command {
    cam_trigger_event() { cmd = CAM_TRIGGER_EVENT; }

    virtual ~cam_trigger_event() {}

    //! string, event name
    std::string event {UnsetString};
    //! time: optional float,  calendar time in UTC, that should be used for
    //! generated event, if it's missing generated event will use the current
    //! timestamp
    double time {UnsetFloat};
    //! meta: optional struct, string key-value mapping with some event metadata
    json meta;

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(cam_trigger_event,
                                       base_command,
                                       event,
                                       time,
                                       meta);
};
}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg

#endif
