#ifndef __COMMAND_REPORT_PROBLEM_H
#define __COMMAND_REPORT_PROBLEM_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

/**
 *  4.16 report_problem (SRV)
 *  Notify CM that the command sent to the server was not processed
 * successfully. reason: string, possible error explanation refid: int, field
 * msgid message that CM sent origin_cmd: string, field cmd message that CM sent
 */
struct report_problem : public base_command {
    report_problem() { cmd = REPORT_PROBLEM; }

    virtual ~report_problem() {}

    // reason: string, possible error explanation
    std::string reason;

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(report_problem, base_command, reason);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
