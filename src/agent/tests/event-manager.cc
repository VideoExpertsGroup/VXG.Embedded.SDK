#include <agent/tests/event-manager.h>

TEST_F(event_manager_test, construct_destruct) {
    EXPECT_NO_THROW();
}

TEST_F(event_manager_test, set_get_events) {
    EXPECT_CALL(*stateless_event_stream_, get_events)
        .WillOnce([&](std::vector<proto::event_config>& config) {
            config.push_back(stateless_event_conf_);
            return true;
        });

    EXPECT_CALL(*stateful_event_stream_, get_events)
        .WillOnce([&](std::vector<proto::event_config>& config) {
            config.push_back(stateful_event_conf_);
            return true;
        });

    EXPECT_CALL(*stateless_event_stream_, set_events)
        .WillOnce([&](const std::vector<proto::event_config>& config) {
            EXPECT_EQ(1U, config.size());
            EXPECT_TRUE(config.begin()->name() == stateless_event_conf_.name());
            EXPECT_TRUE(config.begin()->caps_eq(stateless_event_conf_));
            return true;
        });

    EXPECT_CALL(*stateful_event_stream_, set_events)
        .WillOnce([&](const std::vector<proto::event_config>& config) {
            EXPECT_EQ(1U, config.size());
            EXPECT_TRUE(config.begin()->name() == stateful_event_conf_.name());
            EXPECT_TRUE(config.begin()->caps_eq(stateful_event_conf_));
            return true;
        });

    event_manager_->get_events(events_config_);
    event_manager_->set_events(events_config_);

    EXPECT_NO_THROW();
}

TEST_F(event_manager_test, notify_not_provided) {
    proto::event_object e;
    e.event = proto::ET_MOTION;

    EXPECT_FALSE(event_manager_->notify_event(e));
}

// Universal test covers all cases depending on input params
void event_manager_test::DO_TEST(
    const std::map<mock_event_stream_ptr, proto::events_config>&
        events_settings,
    const std::pair<mock_event_stream_ptr,
                    vxg::cloud::agent::proto::event_config>& test_event_conf) {
    auto now = utils::time::now();
    std::chrono::seconds event_duration = std::chrono::seconds(20);

    for (auto& settings : events_settings) {
        auto event_stream = settings.first;
        auto& events_conf = settings.second.events;

        EXPECT_CALL(*event_stream, get_events)
            .WillOnce([&](std::vector<proto::event_config>& config) {
                for (auto& e : events_conf)
                    config.push_back(e);
                return true;
            });
    }

    // Invokes get_events() for each event stream to inject event configs to
    // event_manager
    proto::events_config all_events;
    event_manager_->get_events(all_events);

    if (test_event_conf.second.caps.stateful) {
        // We notify stateful event start, wait for some time, notify event
        // stop.
        //
        // We do expect the following:
        // - after the event start notification we expect on_event_start().
        // - during the waiting between the event start and stop notifications
        // we expect multiple invokes of the on_event_continue().
        // - after the event stop notification we expect on_event_stop().

        // One on_event_start()
        EXPECT_CALL(*event_handler_cb_, on_event_start)
            .WillOnce(
                Invoke([&](const event_state& state, const cloud::time& start) {
                    EXPECT_TRUE(state.config().name() ==
                                test_event_conf.second.name());
                    EXPECT_EQ(now, start);
                    EXPECT_TRUE(state.active());
                }));

        // One on_event_stop()
        EXPECT_CALL(*event_handler_cb_, on_event_stop)
            .WillOnce(
                Invoke([&](const event_state& state, const cloud::time& stop) {
                    EXPECT_TRUE(state.config().name() ==
                                test_event_conf.second.name());
                    EXPECT_EQ(now + event_duration, stop);
                    EXPECT_FALSE(state.active());
                }));

        // At most (event_duration / continuation_step) times
        // on_event_continue()
        continuation_step_ =
            test_event_conf.second.caps.state_emulation_report_delay;
        continuation_time_ = now + continuation_step_;
        EXPECT_CALL(*event_handler_cb_, on_event_continue)
            .Times(AtMost(event_duration / continuation_step_))
            .WillRepeatedly(
                Invoke([this, test_event_conf](const event_state& state,
                                               const cloud::time& t) {
                    EXPECT_TRUE(state.config().name() ==
                                test_event_conf.second.name());
                    EXPECT_EQ(continuation_time_, t);
                    continuation_time_ += continuation_step_;
                    EXPECT_TRUE(state.active());
                }));
    } else {
        // One on_event_triggered()
        EXPECT_CALL(*event_handler_cb_, on_event_trigger(_, _))
            .WillOnce(
                Invoke([&](const event_state& state, const cloud::time& start) {
                    EXPECT_TRUE(state.config().name() ==
                                test_event_conf.second.name());
                    EXPECT_EQ(now, start);
                    EXPECT_FALSE(state.active());
                }));
    }

    if (test_event_conf.second.caps.stream && test_event_conf.second.stream) {
        std::shared_ptr<void> poison =
            std::make_shared<std::string>(utils::random_string());
        EXPECT_CALL(*event_handler_cb_, on_need_stream_sync_start(_, _))
            .WillOnce(Return(poison));
        EXPECT_CALL(*event_handler_cb_, on_need_stream_sync_stop(_, _, poison));
    }

    // Handle event payload callback(*_info fields handling)
    auto& expectation = EXPECT_CALL(
        handle_event_payload_cb_, Call(_, test_event_conf.second.caps.snapshot &
                                              test_event_conf.second.snapshot));
    if (test_event_conf.second.caps.stateful) {
        // Multiple event payload callbacks, one per notify_event(), and one per
        // each on_event_continue()
        expectation.WillRepeatedly(Return(true));

        // Notify event start
        proto::event_object e;
        e.event = test_event_conf.second.event;
        e.custom_event_name = test_event_conf.second.custom_event_name;
        e.active = true;
        e.time = utils::time::to_double(now);

        event_manager_->notify_event(e);

        // Wait for event duration
        std::this_thread::sleep_for(event_duration);

        // Notify event stop
        e.active = false;
        e.time =
            utils::time::to_double(now + std::chrono::seconds(event_duration));
        EXPECT_TRUE(event_manager_->notify_event(e));
    } else {
        // One event payload callback
        expectation.WillOnce(Return(true));

        // Notify event
        proto::event_object e;
        e.event = test_event_conf.second.event;
        e.custom_event_name = test_event_conf.second.custom_event_name;
        e.time = utils::time::to_double(now);

        event_manager_->notify_event(e);
    }

    // Wait for 1 second to be sure that on_event_stop() called
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

TEST_F(event_manager_test, stateless_no_payload) {
    auto test_case =
        std::make_pair(stateless_event_stream_, stateless_event_conf_);
    DO_TEST(test_config_, test_case);
}

TEST_F(event_manager_test, stateful_no_payload) {
    auto test_case =
        std::make_pair(stateful_event_stream_, stateful_event_conf_);
    DO_TEST(test_config_, test_case);
}

TEST_F(event_manager_test, stateless_snapshot) {
    auto test_case =
        std::make_pair(stateless_event_stream_, stateless_snapshot_event_conf_);
    DO_TEST(test_config_, test_case);
}

TEST_F(event_manager_test, stateful_snapshot) {
    auto test_case =
        std::make_pair(stateful_event_stream_, stateful_snapshot_event_conf_);
    DO_TEST(test_config_, test_case);
}

TEST_F(event_manager_test, stateful_snapshot_disabled) {
    auto test_case = std::make_pair(stateful_event_stream_,
                                    stateful_snapshot_disabled_event_conf_);
    DO_TEST(test_config_, test_case);
}

TEST_F(event_manager_test, stateless_stream) {
    auto test_case =
        std::make_pair(stateless_event_stream_, stateless_stream_event_conf_);
    DO_TEST(test_config_, test_case);
}

TEST_F(event_manager_test, stateful_stream) {
    auto test_case =
        std::make_pair(stateful_event_stream_, stateful_stream_event_conf_);
    DO_TEST(test_config_, test_case);
}

TEST_F(event_manager_test, stateful_stream_disabled) {
    stateful_stream_event_conf_.stream = false;
    auto test_case = std::make_pair(stateful_event_stream_,
                                    stateful_stream_disabled_event_conf_);
    DO_TEST(test_config_, test_case);
}
