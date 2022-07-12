#ifndef __COMMAND_GET_OS_DCONF_H
#define __COMMAND_GET_OS_DCONF_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 3.35 osd_conf (CM)
//! Camera OSD parameters. Reply 3.34 get_osd_conf (SRV).
struct osd_conf : public base_command, public proto::osd_config {
    typedef std::shared_ptr<osd_conf> ptr;

    osd_conf() { cmd = OSD_CONF; }

    virtual ~osd_conf() {}

    //! XXX
    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(osd_conf,
                                                base_command,
                                                proto::osd_config);
};

//! 3.34 get_osd_conf (SRV)
//! Request OSD configuration.
struct get_osd_conf : public base_command {
    typedef std::shared_ptr<get_osd_conf> ptr;

    get_osd_conf() { cmd = GET_OSD_CONF; }

    virtual ~get_osd_conf() {}

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE_NO_ARGS(get_osd_conf, base_command);
};
}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
