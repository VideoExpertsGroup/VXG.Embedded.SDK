#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <agent-proto/command-handler.h>
#include <agent-proto/command/command-factory.h>
#include <agent/manager.h>
#include <net/lws_common.h>
#include <net/websockets.h>
#include <utils/properties.h>

using namespace ::testing;
using namespace vxg::cloud;
using namespace vxg::cloud::agent::proto::command;
using namespace vxg::cloud::transport;

const std::chrono::seconds COMMAND_REPLY_TIMEOUT_SEC = std::chrono::seconds(3);

struct mock_websock_transport
    : public vxg::cloud::transport::libwebsockets::websocket {
    mock_websock_transport() {}
    virtual ~mock_websock_transport() { ; }
    // MOCK_METHOD(bool, start, (), (override));
    // MOCK_METHOD(bool, stop, (), (override));
    MOCK_METHOD(bool, connect, (std::string url), (override));
    MOCK_METHOD(bool, disconnect, (), (override));
    MOCK_METHOD(bool, is_secure, (), (override));
    MOCK_METHOD(bool, send, (const Data& message), (override));

    // virtual void* poll_() override { return nullptr; }
    void on_connected() { connected_(); }
    void on_disconnected() { disconnected_(); }

    void INJECT_COMMAND(json j) {
        std::cout << j.dump() << std::endl;

        rx_q_.push(j);
    }

    void set_handle_cb(std::function<void(transport::Data&)> cb) {
        handle_ = cb;
    }

    template <class C, class REPLY, class RESULT>
    RESULT INJECT_CMD_WAIT_FOR_MATCHED_REPLY_WITH_RESULT(C cmd, REPLY reply) {
        std::promise<RESULT> prom;
        EXPECT_CALL(*this, send(reply))
            .Times(1)
            .WillOnce(testing::Invoke(
                [&prom](const vxg::cloud::transport::Data& d) -> bool {
                    RESULT r = json::parse(d);
                    prom.set_value(r);
                    return true;
                }))
            .RetiresOnSaturation();
        INJECT_COMMAND(cmd);

        auto f = prom.get_future();
        EXPECT_EQ(std::future_status::ready,
                  f.wait_for(COMMAND_REPLY_TIMEOUT_SEC));
        return f.get();
    }
};
using mock_websock_transport_ptr = std::shared_ptr<mock_websock_transport>;

#define INJECT_CMD_WAIT_FOR_MATCHED_REPLY(obj, cmd, reply)                \
    {                                                                     \
        std::promise<void> prom;                                          \
        EXPECT_CALL(*obj, send(reply))                                    \
            .Times(1)                                                     \
            .WillOnce(testing::Invoke(                                    \
                [&prom](const vxg::cloud::transport::Data&) -> bool {     \
                    prom.set_value();                                     \
                    return true;                                          \
                }))                                                       \
            .RetiresOnSaturation();                                       \
        obj->INJECT_COMMAND(cmd);                                         \
        EXPECT_EQ(std::future_status::ready,                              \
                  prom.get_future().wait_for(COMMAND_REPLY_TIMEOUT_SEC)); \
    }

#define WAIT_FOR_COMMAND_MATCH(obj, cmd_match)                            \
    {                                                                     \
        std::promise<void> prom;                                          \
        EXPECT_CALL(*obj, send(cmd_match))                                \
            .Times(1)                                                     \
            .WillOnce(testing::Invoke(                                    \
                [&prom](const vxg::cloud::transport::Data&) -> bool {     \
                    prom.set_value();                                     \
                    return true;                                          \
                }))                                                       \
            .RetiresOnSaturation();                                       \
        EXPECT_EQ(std::future_status::ready,                              \
                  prom.get_future().wait_for(COMMAND_REPLY_TIMEOUT_SEC)); \
    }

class MockTransport : public vxg::cloud::transport::libwebsockets::websocket {
public:
    enum MockTransportState {
        INIT,
        CONNECTING,
        CONNECTED,
        DISCONNECTING,
        DISCONNECTED,
    };

    MockTransport(std::function<void(transport::Data&)> handler,
                  std::function<void()> connected_cb,
                  std::function<void()> disconnected_cb)
        : transport::libwebsockets::websocket(handler) {
        connected_ = connected_cb;
        disconnected_ = disconnected_cb;
        state = INIT;
    }

    virtual bool connect(std::string) override {
        state = CONNECTING;
        return true;
    }

    bool disconnect() override {
        state = DISCONNECTING;
        return true;
    }

    virtual bool start() override { return run(); }

    virtual bool stop() override {
        lws_common::term();

        return true;
    }

    void POLL() { poll_(); }

    void INJECT_COMMAND(base_command* command) {
        json j = json(*command);

        std::cout << j.dump() << std::endl;

        rx_q_.push(j);
    }

    void INJECT_COMMAND(json j) {
        std::cout << j.dump() << std::endl;

        rx_q_.push(j);
    }

    virtual void* poll_() {
        do {
            switch (state) {
                case INIT:
                    break;
                case CONNECTING:
                    state = CONNECTED;
                    connected_();
                    break;
                case DISCONNECTING:
                    state = DISCONNECTED;
                    disconnected_();
                    break;
                default:
                    break;
            }

            usleep(1);
        } while (running_);

        return nullptr;
    }

    MOCK_METHOD(bool, send, (const transport::Data& message), (override));

    MockTransportState state;
    std::function<void()> connected_;
    std::function<void()> disconnected_;
};

class MockHTTPTransport : public vxg::cloud::transport::libwebsockets::http {
public:
    MOCK_METHOD(bool,
                make,
                (vxg::cloud::transport::libwebsockets::http::request_ptr req),
                (override));
    MOCK_METHOD(bool,
                make_blocked,
                (vxg::cloud::transport::libwebsockets::http::request_ptr req),
                (override));
};

using mock_http_transport = MockHTTPTransport;
using mock_http_transport_ptr = std::shared_ptr<mock_http_transport>;