#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <future>

#include <agent/event-state.h>
#include <tests/test-helpers.h>

using ::testing::MockFunction;

using namespace ::testing;
using namespace std;

using namespace vxg;
using namespace vxg::cloud;
using namespace vxg::cloud::agent;

struct mock_event_state_changed_cb
    : public event_state::event_state_changed_cb {
    mock_event_state_changed_cb() {}
    virtual ~mock_event_state_changed_cb() {}

    MOCK_METHOD(void,
                on_started,
                (const event_state& state, const cloud::time&),
                (override));
    MOCK_METHOD(void,
                on_stopped,
                (const event_state& state, const cloud::time&),
                (override));
    MOCK_METHOD(void,
                on_ongoing,
                (const event_state& state, const cloud::time&),
                (override));
    MOCK_METHOD(void,
                on_triggered,
                (const event_state& state, const cloud::time&),
                (override));
};
using mock_event_state_changed_cb_ptr =
    std::shared_ptr<mock_event_state_changed_cb>;

struct event_state_test : public Test {
    agent::proto::event_config stateless_event_conf_;
    agent::proto::event_config stateful_event_conf_;
    event_state_ptr event_state_stateless_;
    event_state_ptr event_state_stateful_;
    mock_event_state_changed_cb_ptr event_state_changed_cb_;
    transport::libwebsockets::http::ptr transport_ {nullptr};
    transport::timed_callback_ptr timed_cb_scheduler_ {nullptr};

    event_state_test() {}
    ~event_state_test() {}

    virtual void SetUp() {
        transport_ = std::make_shared<transport::libwebsockets::http>();
        timed_cb_scheduler_ =
            dynamic_pointer_cast<transport::timed_callback>(transport_);
        transport_->start();

        event_state_changed_cb_ =
            std::make_shared<mock_event_state_changed_cb>();

        stateless_event_conf_.event = proto::ET_CUSTOM;
        stateless_event_conf_.custom_event_name = "test-event-stateless";
        event_state_stateless_ = std::make_shared<event_state>(
            stateless_event_conf_,
            dynamic_pointer_cast<event_state::event_state_changed_cb>(
                event_state_changed_cb_),
            timed_cb_scheduler_);

        stateful_event_conf_.event = proto::ET_CUSTOM;
        stateful_event_conf_.custom_event_name = "test-event-stateful";
        stateful_event_conf_.caps.stateful = true;
        event_state_stateful_ = std::make_shared<event_state>(
            stateful_event_conf_,
            dynamic_pointer_cast<event_state::event_state_changed_cb>(
                event_state_changed_cb_),
            timed_cb_scheduler_);
    }

    void START_WAIT_STARTED(event_state_ptr es, cloud::time t) {
        std::promise<cloud::time> prom;
        EXPECT_CALL(*event_state_changed_cb_, on_started(_, _))
            .Times(1)
            .WillOnce(testing::Invoke(
                [&prom](const event_state& state, const cloud::time& t) {
                    prom.set_value(t);
                }));

        es->start(t);

        EXPECT_EQ(std::future_status::ready,
                  prom.get_future().wait_for(std::chrono::seconds(3)));
    }

    void STOP_WAIT_STOPPED(event_state_ptr es, cloud::time t) {
        std::promise<cloud::time> prom;
        EXPECT_CALL(*event_state_changed_cb_, on_stopped(_, _))
            .Times(1)
            .WillOnce(testing::Invoke(
                [&prom](const event_state& state, const cloud::time& t) {
                    prom.set_value(t);
                }));
        es->stop(t);

        EXPECT_EQ(std::future_status::ready,
                  prom.get_future().wait_for(std::chrono::seconds(3)));
    }

    void WAIT_ONGOING(event_state_ptr es,
                      size_t num,
                      bool should_be_called = true) {
        while (num--) {
            std::promise<cloud::time> prom;

            if (should_be_called) {
                EXPECT_CALL(*event_state_changed_cb_, on_ongoing(_, _))
                    .Times(Exactly(1))
                    .WillOnce(testing::Invoke(
                        [&prom](const event_state& state,
                                const cloud::time& t) { prom.set_value(t); }));
                EXPECT_EQ(std::future_status::ready,
                          prom.get_future().wait_for(std::chrono::seconds(15)));
            } else {
                EXPECT_CALL(*event_state_changed_cb_, on_ongoing(_, _))
                    .Times(Exactly(0));
            }
        }
    }

    void START_WAIT_TRIGGERED(event_state_ptr es, cloud::time t) {
        std::promise<cloud::time> prom;
        EXPECT_CALL(*event_state_changed_cb_, on_triggered(_, _))
            .Times(1)
            .WillOnce(testing::Invoke(
                [&prom](const event_state& state, const cloud::time& t) {
                    prom.set_value(t);
                }));
        es->start(t);

        EXPECT_EQ(std::future_status::ready,
                  prom.get_future().wait_for(std::chrono::seconds(3)));
    }

    virtual void TearDown() { transport_->stop(); }
};