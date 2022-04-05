#ifndef __COMMAND_REGISTER_H
#define __COMMAND_REGISTER_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 2.3 register (CM)
//! Register CM on Server.
struct register_cmd : public base_command {
    typedef std::shared_ptr<register_cmd> ptr;

    register_cmd() { cmd = REGISTER; }

    virtual ~register_cmd() {}

    //! ver : string, CM application version
    std::string ver {UnsetString};
    //! tz  : string, CM time zone name
    std::string tz {UnsetString};
    //! vendor: string, vendor unique name
    std::string vendor {UnsetString};
    //! pwd: string, password stored after initial registration; on the first
    //! call (never registered or reset)
    std::string pwd {UnsetString};
    //! prev_sid: string optional, previous session ID. Please see 2.4 hello
    //! (SRV)/sid.
    std::string prev_sid {UnsetString};
    //! reg_token: optional string, registration token value. User requests if
    //! from server and passes to CM for initial registration or after CM
    //! settigns reset. The token has a limited life time. Invalid or expired
    //! token value will cause closing of connection 3.13 bye (SRV, CM) with
    //! AUTH_FAILURE reason.
    std::string reg_token {UnsetString};
    //! media_protocols: optional list of string, list of supported media
    //! protocols in preference order; supported variants are rtmp and rtmps, if
    //! not specified rtmp is implied; see 7 Data channel for list of supported
    //! protocols
    std::vector<std::string> media_protocols {};

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(register_cmd,
                                       base_command,
                                       ver,
                                       tz,
                                       vendor,
                                       pwd,
                                       prev_sid,
                                       reg_token,
                                       media_protocols);
};
}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
