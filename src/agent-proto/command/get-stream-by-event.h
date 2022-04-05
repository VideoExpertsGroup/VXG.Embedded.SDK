#ifndef __COMMAND_GET_STREAM_BY_EVENT_H
#define __COMMAND_GET_STREAM_BY_EVENT_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 4.8 set_stream_by_event (SRV)
//! Configure stream start by camera event.
struct stream_by_event_conf : public base_command,
                              public proto::stream_by_event_config {
    typedef std::shared_ptr<stream_by_event_conf> ptr;

    stream_by_event_conf() { cmd = STREAM_BY_EVENT_CONF; }

    virtual ~stream_by_event_conf() {}

    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(stream_by_event_conf,
                                                base_command,
                                                proto::stream_by_event_config);
};

//! 4.7 get_stream_by_event (SRV)
//! Get parameters of starting stream by event.
struct get_stream_by_event : public base_command {
    typedef std::shared_ptr<get_stream_by_event> ptr;

    get_stream_by_event() { cmd = GET_STREAM_BY_EVENT; }

    virtual ~get_stream_by_event() {}

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE_NO_ARGS(get_stream_by_event,
                                               base_command);
};
}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
