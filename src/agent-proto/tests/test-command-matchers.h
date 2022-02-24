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

MATCHER(CommandCamRegisterOK, "Command Register") {
    proto::command::register_cmd command = json::parse(arg);
    return command.cmd == proto::command::CAM_REGISTER;
}
