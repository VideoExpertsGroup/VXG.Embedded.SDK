#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <future>

#include <agent-proto/tests/test-command-handler.h>
#include <agent-proto/tests/test-command-matchers.h>
#include <tests/test-helpers.h>
#include <tests/test_Transport.h>

using namespace ::testing;
using namespace std;

using namespace vxg::cloud::transport;
using namespace vxg::cloud::agent;
using namespace vxg::cloud::agent::proto;

class CommandHandlerTest : public Test, public MockCommandHandler {
protected:
    CommandHandlerTest()
        : MockCommandHandler(std::make_shared<MockTransport>(
              std::bind(&command_handler::on_receive, this, placeholders::_1),
              std::bind(&MockCommandHandler::on_connected, this),
              std::bind(&MockCommandHandler::on_disconnected, this))) {
        cout << endl;
    }

    virtual void SetUp() {}

    virtual void TearDown() {}
};

#define INJECT_CMD_WAIT_FOR_REPLY(obj, cmd, reply)                      \
    {                                                                   \
        std::promise<void> prom;                                        \
        EXPECT_CALL(obj, send(reply))                                   \
            .Times(1)                                                   \
            .WillOnce(testing::Invoke(                                  \
                [&prom](const vxg::cloud::transport::Data&) -> bool {   \
                    prom.set_value();                                   \
                    return true;                                        \
                }));                                                    \
        mockTransport_->INJECT_COMMAND(cmd);                            \
        EXPECT_EQ(std::future_status::ready,                            \
                  prom.get_future().wait_for(std::chrono::seconds(3))); \
    }

TEST_F(CommandHandlerTest, WAIT_FOR_CALL) {
    this->transport_->start();
    WAIT_FOR_CALL(*this, on_connected, this->transport_->connect(""));
    WAIT_FOR_CALL(*this, on_disconnected, this->transport_->disconnect());

    this->transport_->stop();
}

class CommandHandlerTestAsync : public Test, public MockCommandHandler {
protected:
    CommandHandlerTestAsync()
        : MockCommandHandler(std::make_shared<MockTransport>(
              std::bind(&command_handler::on_receive, this, placeholders::_1),
              std::bind(&MockCommandHandler::on_connected, this),
              std::bind(&MockCommandHandler::on_disconnected, this))) {
        mockTransport_ = dynamic_pointer_cast<MockTransport>(this->transport_);
    }

    virtual void SetUp() {
        transport_->start();
        // Expect register on connect
        EXPECT_CALL(*mockTransport_, send(CommandRegisterOK())).Times(1);

        WAIT_FOR_CALL_AND_DO(*this, on_connected, transport_->connect(""),
                             this->do_register);
    }

    virtual void TearDown() {
        WAIT_FOR_CALL(*this, on_disconnected, transport_->disconnect());
        transport_->stop();
    }

    std::shared_ptr<MockTransport> mockTransport_;
};

TEST_F(CommandHandlerTest, OnConnectedDisconnected) {
    EXPECT_CALL(*this, on_connected());
    this->transport_->connect("");
    dynamic_pointer_cast<MockTransport>(this->transport_)->POLL();

    EXPECT_CALL(*this, on_disconnected());
    this->transport_->disconnect();
    dynamic_pointer_cast<MockTransport>(this->transport_)->POLL();
}

// Inject Hello
// Expect done
// Expect cam_register
TEST_F(CommandHandlerTestAsync, CommandRegisterOnConnect) {
    hello hello;
    bye bye;
    bye.reason = BR_SHUTDOWN;

    // Expect done for hello
    EXPECT_CALL(*mockTransport_, send(CommandDoneOK())).Times(1);

    // Wait & Expect cam_register command as response to injected hello
    INJECT_CMD_WAIT_FOR_REPLY(*mockTransport_, hello, CommandCamRegisterOK());
}

TEST_F(CommandHandlerTestAsync, supported_streams) {
    get_supported_streams getSupportedStreams;
    json jcmd = getSupportedStreams;

    // Mock success on_get_supported_streams()
    EXPECT_CALL(*this, on_get_supported_streams(_)).WillOnce(Return(true));

    // Inject getSupportedStreams & wait for done failure
    INJECT_CMD_WAIT_FOR_REPLY(*mockTransport_, getSupportedStreams,
                              CommandSupportedStreamsOk());
}

TEST_F(CommandHandlerTestAsync, CommandSupportedStreamsFail) {
    get_supported_streams getSupportedStreams;

    // Mock failed on_get_supported_streams()
    EXPECT_CALL(*this, on_get_supported_streams(_)).WillOnce(Return(false));

    // Inject getSupportedStreams & wait for done failure
    INJECT_CMD_WAIT_FOR_REPLY(*mockTransport_, getSupportedStreams,
                              CommandDoneFail());
}

TEST_F(CommandHandlerTestAsync, configure) {
    configure commandConfigure;
    commandConfigure.server = "__test_server_url__";
    commandConfigure.pwd = "__test_pwd__";
    commandConfigure.connid = "__test_connection__";
    commandConfigure.uuid = "__test_uuid__";

    // Inject configure & wait for done
    INJECT_CMD_WAIT_FOR_REPLY(*mockTransport_, commandConfigure,
                              CommandDoneOK());
}
