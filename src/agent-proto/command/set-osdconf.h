#ifndef __COMMAND_SET_OS_DCONF_H
#define __COMMAND_SET_OS_DCONF_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 3.36 set_osd_conf (SRV)
//! Changes camera OSD parameters. Format is equal to 3.35 osd_conf (CM)
//! command.
struct set_osd_conf : public base_command, public proto::osd_config {
    set_osd_conf() { cmd = SET_OSD_CONF; }

    virtual ~set_osd_conf() {}

    //! XXX
    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(set_osd_conf,
                                                base_command,
                                                proto::osd_config);
};
}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
