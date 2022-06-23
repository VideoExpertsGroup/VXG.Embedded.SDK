#include <gtest/gtest.h>

#include <agent-proto/proto.h>

using namespace vxg::cloud::agent::proto::command;

TEST(base_command, DefaultConstructor) {
    // Arrange
    base_command cmd;

    // Act

    // Assert
    EXPECT_STREQ(UnsetString.c_str(), cmd.name().c_str());
    EXPECT_EQ(UnsetInt, cmd.camid());
    EXPECT_EQ(UnsetInt, cmd.ref_id());
    EXPECT_LT(1, cmd.id());
}

TEST(base_command, Constructor) {
    // Arrange
    base_command cmd("test_cmd", 44, 55, "orig_cmd");

    // Act

    // Assert
    EXPECT_STREQ("test_cmd", cmd.name().c_str());
    EXPECT_STREQ("orig_cmd", cmd.orig_name().c_str());

    EXPECT_EQ(55, cmd.refid);
    EXPECT_EQ(44, cmd.cam_id);

    EXPECT_LT(0, cmd.msgid);

    EXPECT_EQ(55, cmd.ref_id());
    EXPECT_EQ(44, cmd.camid());
    EXPECT_LT(0, cmd.id());
}

TEST(base_command, JSONConstructor) {
    // Arrange
    json j = {{"cmd", "test_cmd"},
              {"msgid", 33},
              {"cam_id", 44},
              {"refid", 55},
              {"orig_cmd", "orig_test_cmd"}};
    base_command cmd = j;

    // Act

    // Assert
    EXPECT_STREQ("test_cmd", cmd.name().c_str());
    EXPECT_STREQ("orig_test_cmd", cmd.orig_name().c_str());

    EXPECT_EQ(55, cmd.refid);
    EXPECT_EQ(44, cmd.cam_id);

    EXPECT_EQ(33, cmd.msgid);

    EXPECT_EQ(55, cmd.ref_id());
    EXPECT_EQ(44, cmd.camid());
    EXPECT_EQ(33, cmd.id());
}

TEST(base_command, OperatorSet) {
    // Arrange
    base_command cmd1("test_cmd", 44, 55);

    // Act
    base_command cmd2 = cmd1;

    // Assert
    // test if cmd1 wasn't changed
    EXPECT_STREQ("test_cmd", cmd2.name().c_str());
    EXPECT_EQ(55, cmd1.ref_id());
    EXPECT_EQ(44, cmd1.camid());
    EXPECT_LT(0, cmd1.id());

    EXPECT_EQ(cmd2.cam_id, cmd1.cam_id);
    EXPECT_EQ(cmd2.refid, cmd1.refid);
    EXPECT_EQ(cmd2.msgid, cmd1.msgid);

    EXPECT_EQ(cmd2.camid(), cmd1.camid());
    EXPECT_EQ(cmd2.ref_id(), cmd1.ref_id());
    EXPECT_EQ(cmd2.id(), cmd1.id());
}

TEST(base_command, done) {
    // Arrange
    base_command orig_cmd("test_cmd", 55);

    // Act
    base_command::ptr donecommand = factory::done(&orig_cmd, DS_MISSED_PARAM);
    done* done_cmd = (done*)donecommand.get();
    json j = *std::dynamic_pointer_cast<done>(donecommand).get();

    std::cout << j.dump() << std::endl;

    // Assert
    EXPECT_STREQ("done", done_cmd->name().c_str());
    EXPECT_EQ(DS_MISSED_PARAM, j["status"].get<done_status>());

    // same camid
    EXPECT_EQ(orig_cmd.cam_id, done_cmd->cam_id);
    // done refid is the msgid of original command
    EXPECT_EQ(orig_cmd.msgid, done_cmd->refid);
    // done msgid is not equal to original command
    EXPECT_NE(orig_cmd.msgid, done_cmd->msgid);
    // same camid
    EXPECT_EQ(orig_cmd.camid(), done_cmd->camid());
    // done refid is the msgid of original command
    EXPECT_EQ(orig_cmd.id(), done_cmd->ref_id());
    // done msgid is not equal to original command
    EXPECT_NE(orig_cmd.id(), done_cmd->id());
}

TEST(base_command, reply) {
    // Arrange
    base_command orig_cmd("test_cmd", 3, 55);

    // Act
    base_command reply_cmd = orig_cmd.reply();

    // Assert
    EXPECT_STREQ(UnsetString.c_str(), reply_cmd.name().c_str());
    // orig_name is the name of original command
    EXPECT_STREQ(orig_cmd.name().c_str(), reply_cmd.orig_name().c_str());
    // same camid
    EXPECT_EQ(orig_cmd.cam_id, reply_cmd.cam_id);
    // done refid is the msgid of original command
    EXPECT_EQ(orig_cmd.msgid, reply_cmd.refid);
    // done msgid is greater than original command
    EXPECT_LT(orig_cmd.msgid, reply_cmd.msgid);
    // same camid
    EXPECT_EQ(orig_cmd.camid(), reply_cmd.camid());
    // done refid is the msgid of original command
    EXPECT_EQ(orig_cmd.id(), reply_cmd.ref_id());
    // done msgid is greater than original command
    EXPECT_LT(orig_cmd.id(), reply_cmd.id());
}

TEST(base_command, ReplyWithCommandName) {
    // Arrange
    base_command orig_cmd("test_cmd", 3, 55);

    // Act
    base_command reply_cmd = orig_cmd.reply("test-cmd-reply");

    // Assert
    EXPECT_STREQ("test-cmd-reply", reply_cmd.name().c_str());
    // orig_name is the name of original command
    EXPECT_STREQ(orig_cmd.name().c_str(), reply_cmd.orig_name().c_str());
    // same camid
    EXPECT_EQ(orig_cmd.cam_id, reply_cmd.cam_id);
    // done refid is the msgid of original command
    EXPECT_EQ(orig_cmd.msgid, reply_cmd.refid);
    // done msgid is greater than original command
    EXPECT_LT(orig_cmd.msgid, reply_cmd.msgid);
    // same camid
    EXPECT_EQ(orig_cmd.camid(), reply_cmd.camid());
    // done refid is the msgid of original command
    EXPECT_EQ(orig_cmd.id(), reply_cmd.ref_id());
    // done msgid is greater than original command
    EXPECT_LT(orig_cmd.id(), reply_cmd.id());
}

TEST(base_command, Empty) {
    // Arrange
    base_command cmd_empty;
    base_command cmd_nonempty("test_cmd1");

    // Act

    // Assert
    EXPECT_TRUE(cmd_empty.empty());
    EXPECT_FALSE(cmd_nonempty.empty());
}

TEST(base_command, ToJSON) {
    // Arrange
    base_command cmd;
    json j({{"cmd", "test_cmd1"}, {"msgid", 33}, {"refid", 44}});

    // Act
    cmd.cmd = j["cmd"];
    cmd.msgid = j["msgid"];
    cmd.refid = j["refid"];

    std::string s1 = j.dump();
    std::string s2 = json(cmd).dump();
    // Assert
    EXPECT_TRUE(json(cmd).dump() == j.dump());
}

TEST(base_command, register) {
    std::shared_ptr<base_command> regCommand = factory::create(REGISTER);
    std::shared_ptr<register_cmd> command =
        std::dynamic_pointer_cast<register_cmd>(regCommand);

    command->ver = "1";
    command->tz = "GTM+7";
    command->vendor = "VXG";
    command->pwd = "password";
    // command->prev_sid = "";
    command->reg_token = "token";
    command->media_protocols.push_back("rtmps");

    *regCommand = *command;

    spdlog::info("register: {}", json(*regCommand.get()).dump().c_str());
    std::shared_ptr<register_cmd> command2 =
        std::dynamic_pointer_cast<register_cmd>(regCommand);
    json j = *command.get();
    json jj = *command2.get();
    std::string s = j.dump();
    std::string ss = jj.dump();

    EXPECT_STREQ(j["tz"].get<std::string>().c_str(),
                 jj["tz"].get<std::string>().c_str());
}

#include <nlohmann/json.hpp>
struct ca {
    int a {1};

    JSON_DEFINE_TYPE_INTRUSIVE(ca, a);
};

struct cb : public ca {
    int b {2};

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(cb, ca, b);
};

TEST(nlohmann_json, JSON_DEFINE_DERIVED_TYPE_INTRUSIVE) {
    cb ccb;
    json j = {{"b", 2}, {"a", 1}};
    json jccb = ccb;

    EXPECT_TRUE(jccb.dump() == j.dump());
}