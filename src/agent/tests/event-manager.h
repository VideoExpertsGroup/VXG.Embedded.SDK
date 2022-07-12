#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <future>

#include <agent/config.h>
#include <agent/event-manager.h>

#include <tests/test-helpers.h>

using namespace ::testing;
using ::testing::MockFunction;
using namespace std;

using namespace vxg;
using namespace vxg::cloud;
using namespace vxg::cloud::agent;

struct mock_event_handler_cb : public event_manager::event_state_report_cb {
    mock_event_handler_cb() : event_manager::event_state_report_cb() {}
    virtual ~mock_event_handler_cb() {}

    MOCK_METHOD(void,
                on_event_start,
                (const event_state& state, const cloud::time&),
                (override));
    MOCK_METHOD(void,
                on_event_stop,
                (const event_state& state, const cloud::time&),
                (override));
    MOCK_METHOD(void,
                on_event_trigger,
                (const event_state& state, const cloud::time&),
                (override));
    MOCK_METHOD(void,
                on_event_continue,
                (const event_state& state, const cloud::time&),
                (override));

    MOCK_METHOD(std::shared_ptr<void>,
                on_need_stream_sync_start,
                (const event_state& state, const cloud::time&),
                (override));
    MOCK_METHOD(void,
                on_need_stream_sync_stop,
                (const event_state& state,
                 const cloud::time&,
                 std::shared_ptr<void>),
                (override));
};
using mock_event_handler_ptr = std::shared_ptr<mock_event_handler_cb>;

struct mock_event_stream : public event_stream {
    agent::proto::events_config config_;

    mock_event_stream(std::string name) : event_stream(name) {}
    virtual ~mock_event_stream() {}

    MOCK_METHOD(bool, start, (), (override));
    MOCK_METHOD(void, stop, (), (override));

    MOCK_METHOD(bool, init, (), (override));
    MOCK_METHOD(void, finit, (), (override));
    MOCK_METHOD(bool,
                get_events,
                (std::vector<proto::event_config> & configs),
                (override));
    MOCK_METHOD(bool,
                set_events,
                (const std::vector<proto::event_config>& config),
                (override));
    MOCK_METHOD(bool,
                set_trigger_recording,
                (bool enabled, int pre, int post),
                (override));
};
using mock_event_stream_ptr = std::shared_ptr<mock_event_stream>;

struct event_manager_test : public Test {
    agent::proto::event_config stateless_event_conf_;
    agent::proto::event_config stateful_event_conf_;
    agent::proto::event_config stateless_snapshot_event_conf_;
    agent::proto::event_config stateful_snapshot_event_conf_;
    agent::proto::event_config stateful_snapshot_disabled_event_conf_;
    agent::proto::event_config stateless_stream_event_conf_;
    agent::proto::event_config stateful_stream_event_conf_;
    agent::proto::event_config stateful_stream_disabled_event_conf_;

    agent::proto::events_config events_config_;
    event_manager_ptr event_manager_;
    mock_event_handler_ptr event_handler_cb_;
    mock_event_stream_ptr stateful_event_stream_;
    mock_event_stream_ptr stateless_event_stream_;
    std::map<mock_event_stream_ptr, proto::events_config> test_config_;
    cloud::duration continuation_step_;
    cloud::time continuation_time_;

    MockFunction<event_manager::handle_event_payload_cb>
        handle_event_payload_cb_;
    bool wrap_handle_event_payload(agent::proto::event_object& e,
                                   bool need_snapshot) {
        return handle_event_payload_cb_.AsStdFunction()(e, need_snapshot);
    }

    event_manager_test() {}
    virtual ~event_manager_test() {}

    virtual void SetUp() {
        // stateless no payload
        stateless_event_conf_.active = true;
        stateless_event_conf_.event = proto::ET_CUSTOM;
        stateless_event_conf_.custom_event_name = "test-event-stateless";

        // stateful no payload
        stateful_event_conf_.event = proto::ET_CUSTOM;
        stateful_event_conf_.custom_event_name = "test-event-stateful";
        stateful_event_conf_.active = true;
        stateful_event_conf_.caps.stateful = true;
        stateful_event_conf_.caps.state_emulation_report_delay =
            std::chrono::seconds(1);

        // stateless snapshot
        stateless_snapshot_event_conf_.active = true;
        stateless_snapshot_event_conf_.event = proto::ET_CUSTOM;
        stateless_snapshot_event_conf_.custom_event_name =
            "test-event-stateless-snapshot";
        stateless_snapshot_event_conf_.snapshot = true;
        stateless_snapshot_event_conf_.caps.snapshot = true;

        // stateful snapshot
        stateful_snapshot_event_conf_.event = proto::ET_CUSTOM;
        stateful_snapshot_event_conf_.custom_event_name =
            "test-event-stateful-snapshot";
        stateful_snapshot_event_conf_.active = true;
        stateful_snapshot_event_conf_.caps.stateful = true;
        stateful_snapshot_event_conf_.caps.state_emulation_report_delay =
            std::chrono::seconds(1);
        stateful_snapshot_event_conf_.snapshot = true;
        stateful_snapshot_event_conf_.caps.snapshot = true;

        // stateful snapshot disabled
        stateful_snapshot_disabled_event_conf_.event = proto::ET_CUSTOM;
        stateful_snapshot_disabled_event_conf_.custom_event_name =
            "test-event-stateful-snapshot-disabled";
        stateful_snapshot_disabled_event_conf_.active = true;
        stateful_snapshot_disabled_event_conf_.caps.stateful = true;
        stateful_snapshot_disabled_event_conf_.caps
            .state_emulation_report_delay = std::chrono::seconds(1);
        // actual disabling, still presented in caps
        stateful_snapshot_disabled_event_conf_.snapshot = false;
        stateful_snapshot_disabled_event_conf_.caps.snapshot = true;

        // stateless stream
        stateless_stream_event_conf_.active = true;
        stateless_stream_event_conf_.event = proto::ET_CUSTOM;
        stateless_stream_event_conf_.custom_event_name =
            "test-event-stateless-stream";
        stateless_stream_event_conf_.stream = true;
        stateless_stream_event_conf_.caps.stream = true;

        // stateful stream
        stateful_stream_event_conf_.event = proto::ET_CUSTOM;
        stateful_stream_event_conf_.custom_event_name =
            "test-event-stateful-stream";
        stateful_stream_event_conf_.active = true;
        stateful_stream_event_conf_.caps.stateful = true;
        stateful_stream_event_conf_.caps.state_emulation_report_delay =
            std::chrono::seconds(1);
        stateful_stream_event_conf_.stream = true;
        stateful_stream_event_conf_.caps.stream = true;

        // stateful stream disabled
        stateful_stream_disabled_event_conf_.event = proto::ET_CUSTOM;
        stateful_stream_disabled_event_conf_.custom_event_name =
            "test-event-stateful-stream-disabled";
        stateful_stream_disabled_event_conf_.active = true;
        stateful_stream_disabled_event_conf_.caps.stateful = true;
        stateful_stream_disabled_event_conf_.caps.state_emulation_report_delay =
            std::chrono::seconds(1);
        // actual disabling stream, still presented in caps
        stateful_stream_disabled_event_conf_.stream = false;
        stateful_stream_disabled_event_conf_.caps.stream = true;

        event_handler_cb_ = std::make_shared<mock_event_handler_cb>();
        stateful_event_stream_ =
            std::make_shared<mock_event_stream>("stateful-event-stream");
        stateless_event_stream_ =
            std::make_shared<mock_event_stream>("stateless-event-stream");

        // Test config
        test_config_[stateful_event_stream_].events = {
            stateful_event_conf_, stateful_snapshot_event_conf_,
            stateful_stream_event_conf_, stateful_stream_disabled_event_conf_,
            stateful_snapshot_disabled_event_conf_};

        test_config_[stateless_event_stream_].events = {
            stateless_event_conf_, stateless_snapshot_event_conf_,
            stateless_stream_event_conf_};

        // event-manager
        std::vector<std::shared_ptr<agent::event_stream>> event_streams;
        event_streams.push_back(stateful_event_stream_);
        event_streams.push_back(stateless_event_stream_);

        event_manager_ = std::make_shared<agent::event_manager>(
            agent::config(), event_streams, event_handler_cb_,
            std::bind(&event_manager_test::wrap_handle_event_payload, this,
                      std::placeholders::_1, std::placeholders::_2));

        event_manager_->start();

        // Always expect multiple start/stop for events streams
        EXPECT_CALL(*stateless_event_stream_, start())
            .WillRepeatedly(Return(true));
        EXPECT_CALL(*stateless_event_stream_, stop()).WillRepeatedly(Return());

        EXPECT_CALL(*stateful_event_stream_, start())
            .WillRepeatedly(Return(true));
        EXPECT_CALL(*stateful_event_stream_, stop()).WillRepeatedly(Return());
    }

    virtual void TearDown() { event_manager_->stop(); }

    void EXPECT_EVENT_PAYLOAD_HANDLE(proto::event_object e,
                                     bool need_snapshot) {}

    void DO_TEST(const std::map<mock_event_stream_ptr, proto::events_config>&
                     events_settings,
                 const std::pair<mock_event_stream_ptr, proto::event_config>&
                     test_event_conf);
};
