#ifndef __COMMAND_STREAM_STOP_H
#define __COMMAND_STREAM_STOP_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 4.3 stream_stop (SRV)
//! Stop camera stream. CM must be sure that server doesn't need the stream for
//! any other purpose before stop the stream
struct stream_stop : public base_command {
    stream_stop() { cmd = STREAM_STOP; }

    virtual ~stream_stop() {}

    //! reason of stream staring.
    proto::stream_reason reason {proto::stream_reason::SR_INVALID};
    //! string, camera media stream to use
    std::string stream_id {UnsetString};

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(stream_stop,
                                       base_command,
                                       reason,
                                       stream_id);
};
}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
