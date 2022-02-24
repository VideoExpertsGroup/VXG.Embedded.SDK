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
using namespace std;
using namespace vxg::cloud;
using namespace vxg::cloud::agent::proto::command;

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
