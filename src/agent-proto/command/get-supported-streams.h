#ifndef __COMMAND_GET_SUPPORTED_STREAMS_H
#define __COMMAND_GET_SUPPORTED_STREAMS_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

struct supported_streams : public base_command,
                           public proto::supported_streams_config {
    typedef std::shared_ptr<supported_streams> ptr;

    supported_streams() { cmd = SUPPORTED_STREAMS; }

    supported_streams(proto::supported_streams_config& conf)
        : supported_streams_config(conf) {
        cmd = SUPPORTED_STREAMS;
    }

    virtual ~supported_streams() {}

    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(
        supported_streams,
        base_command,
        proto::supported_streams_config);
};

struct get_supported_streams : public base_command {
    typedef std::shared_ptr<get_supported_streams> ptr;

    get_supported_streams() { cmd = GET_SUPPORTED_STREAMS; }

    virtual ~get_supported_streams() {}

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE_NO_ARGS(get_supported_streams,
                                               base_command);
};
}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
