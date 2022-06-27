#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <agent-proto/proto.h>

using namespace vxg::cloud::agent;

MATCHER(CommandRegisterOK, "Command Register") {
    proto::command::done command = json::parse(arg);
    return command.cmd == proto::command::REGISTER;
}

MATCHER(CommandDoneOK, "Command done with status OK") {
    proto::command::done command = json::parse(arg);
    *result_listener << "\nStatus: " << command.status;
    *result_listener << "\ncmd: " << command.cmd;
    return command.cmd == proto::command::DONE &&
           command.status == proto::command::DS_OK;
}

MATCHER_P(command_reply_done_OK,
          orig_cmd,
          "Command done with status OK to original cmd") {
    proto::command::done command = json::parse(arg);
    *result_listener << "\nStatus: " << command.status;
    *result_listener << "\ncmd: " << command.cmd;
    *result_listener << "\norig_cmd: " << command.orig_cmd;

    return command.cmd == proto::command::DONE &&
           command.orig_cmd == orig_cmd &&
           command.status == proto::command::DS_OK;
}

MATCHER_P2(command_reply, orig_cmd, reply_cmd, "Command to original cmd") {
    proto::command::base_command command = json::parse(arg);
    *result_listener << "\ncmd: " << command.cmd;
    *result_listener << "\norig_cmd: " << command.orig_cmd;

    return command.cmd == reply_cmd && command.orig_cmd == orig_cmd;
}

MATCHER(CommandDoneFail, "Command done with status != OK") {
    proto::command::done command = json::parse(arg);
    return command.cmd == proto::command::DONE &&
           command.status != proto::command::DS_OK;
}

MATCHER(CommandSupportedStreamsOk, "Command supported_streams") {
    proto::command::done command = json::parse(arg);
    return command.cmd == proto::command::SUPPORTED_STREAMS &&
           command.status != proto::command::DS_OK;
}

MATCHER(is_cam_register_command, "Command Cam Register") {
    proto::command::base_command command = json::parse(arg);
    return command.cmd == proto::command::CAM_REGISTER;
}

MATCHER_P(is_command, cmd, "Is command name matches") {
    proto::command::base_command command = json::parse(arg);
    return command.cmd == cmd;
}

MATCHER_P(is_event, event_name, "Is cam_event name matches") {
    proto::event_object event = json::parse(arg);
    return event.name() == event_name;
}

MATCHER_P(is_string_contains, substring, "Is string includes substring") {
    return vxg::cloud::utils::string_contains(arg, substring);
}
