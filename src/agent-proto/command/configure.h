#ifndef __COMMAND_CONFIGURE_H
#define __COMMAND_CONFIGURE_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

struct configure : public base_command {
    typedef std::shared_ptr<configure> ptr;

    configure() { cmd = CONFIGURE; }

    virtual ~configure() {}

    std::string pwd {UnsetString};
    std::string uuid {UnsetString};
    std::string connid {UnsetString};
    std::string tz {UnsetString};
    std::string server {UnsetString};

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(configure,
                                       base_command,
                                       pwd,
                                       uuid,
                                       connid,
                                       tz,
                                       server);
};
}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
