#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <future>

#include <agent/manager.h>
#include <net/transport.h>

#include <agent-proto/tests/test-command-handler.h>
#include <agent-proto/tests/test-command-matchers.h>
#include <tests/test_Transport.h>

using namespace ::testing;
using namespace std;

using namespace vxg::cloud::transport;
using namespace vxg::cloud::agent;
using namespace vxg::cloud::agent::proto;

const std::string TEST_ACCESS_TOKEN =
    "eyJjYW1pZCI6IDI0NjI4MywgImNtbmdyaWQiOiAyNDY3MjAsICJhY2Nlc3MiOiAiYWxsIiwgIn"
    "Rva2VuIjogInNoYXJlLmV5SnphU0k2SURJek1Ea3dPSDAuNjE4ZTI5NTB0YmMxZmFiMDAuVXg2"
    "SGtUYlExR05MZlZRR3E3LURoWDRFUWpNIiwgImFwaSI6ICJ3ZWIuc2t5dnIudmlkZW9leHBlcn"
    "RzZ3JvdXAuY29tIiwgImNhbSI6ICJjYW0uc2t5dnIudmlkZW9leHBlcnRzZ3JvdXAuY29tIn0"
    "=";

class mock_callback : public agent::callback {
    MOCK_METHOD(void, on_bye, (proto::command::bye_reason reason), (override));
};

class mock_agent_manager : public agent::manager {
public:
    mock_agent_manager(agent::callback::ptr callback,
                       transport::worker::ptr transport)
        : agent::manager(std::move(callback), transport) {}

    static std::shared_ptr<mock_agent_manager> create(
        agent::callback::ptr callback,
        transport::worker::ptr transport,
        agent::proto::access_token::ptr token) {
        auto result = std::make_shared<mock_agent_manager>(std::move(callback),
                                                           transport);

        return result->set_token(*token) ? std::move(result) : nullptr;
    }
};

class agent_manager_test : public Test {
public:
    agent::callback::ptr callback_;
    agent::manager::ptr manager_;
    agent::proto::access_token::ptr token_;
    std::shared_ptr<MockTransport> transport_;

protected:
    agent_manager_test() {}
    virtual ~agent_manager_test() {}

    virtual void SetUp() {
        transport_ = std::make_shared<MockTransport>([](transport::Data&) {},
                                                     []() {}, []() {});
        token_ = proto::access_token::parse(TEST_ACCESS_TOKEN);
        manager_ = mock_agent_manager::create(std::make_unique<mock_callback>(),
                                              transport_, token_);
        manager_->start();
    }
    virtual void TearDown() { manager_->stop(); }
};