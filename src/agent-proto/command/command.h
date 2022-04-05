#ifndef __COMMAND_H
#define __COMMAND_H

#include <nlohmann/json.hpp>

#include <agent-proto/command/utils.h>

using json = nlohmann::json;
namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {
struct base_command {
    typedef std::shared_ptr<base_command> ptr;

    base_command(std::string _cmd = UnsetString,
                 int _camid = UnsetInt,
                 int _refid = UnsetInt,
                 std::string _orig_cmd = UnsetString)
        : cmd(_cmd),
          msgid(++__id),
          cam_id(_camid),
          refid(_refid),
          orig_cmd(_orig_cmd) {}

    base_command reply(std::string new_cmd = UnsetString) {
        return base_command(new_cmd, cam_id, msgid, cmd);
    }

    virtual ~base_command() {}

    std::string name() { return cmd; }
    std::string orig_name() { return orig_cmd; }
    int id() { return msgid; }
    int camid() { return cam_id; }
    int ref_id() { return refid; }
    bool empty() { return cmd == UnsetString; }

    JSON_DEFINE_TYPE_INTRUSIVE(base_command,
                               cmd,
                               msgid,
                               cam_id,
                               refid,
                               orig_cmd);
    static size_t __id;

    std::string cmd {UnsetString};
    int msgid {UnsetInt};
    int cam_id {UnsetInt};
    int refid {UnsetInt};
    std::string orig_cmd {UnsetString};
};

/*! Done command status */
enum done_status {
    DS_OK,            /**< Success. */
    DS_CM_ERROR,      /**< General Error. */
    DS_SYSTEM_ERROR,  /**< System failure. */
    DS_NOT_SUPPORTED, /**< Functionality is not supported. */
    DS_INVALID_PARAM, /**< Some parameter in packet is invalid. */
    DS_MISSED_PARAM,  /**< Mandatory parameter is missed in the packet. */
    DS_TOO_MANY,      /**< List contains too many elements. */
    DS_RETRY,         /**< Peer is busy, retry later. */

    DS_INVALID = -1,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( done_status, {
    {DS_OK, "OK"},
    {DS_CM_ERROR, "CM_ERROR"},
    {DS_SYSTEM_ERROR, "SYSTEM_ERROR"},
    {DS_INVALID_PARAM, "INVALID_PARAM"},
    {DS_MISSED_PARAM, "MISSED_PARAM"},
    {DS_TOO_MANY, "TOO_MANY"},
    {DS_RETRY, "RETRY"},

    {DS_INVALID, nullptr},
})
// clang-format on

struct done : public base_command {
    typedef std::shared_ptr<done> ptr;

    done() {};

    done(base_command* command, done_status _status = DS_INVALID)
        : base_command("done",
                       command->camid(),
                       command->id(),
                       command->name()),
          status(_status) {}

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(done, base_command, status);

    done_status status {DS_INVALID};
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg

#endif