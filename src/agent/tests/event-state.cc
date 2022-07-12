#include <agent/tests/event-state.h>

using namespace std::chrono;

TEST_F(event_state_test, construct_destruct) {
    EXPECT_NO_THROW();
}

TEST_F(event_state_test, statefull_start_ongoing_stop_callbacks) {
    START_WAIT_STARTED(event_state_stateful_, cloud::utils::time::now());
    WAIT_ONGOING(event_state_stateful_, 1);
    STOP_WAIT_STOPPED(event_state_stateful_, cloud::utils::time::now());

    EXPECT_NO_THROW();
}

TEST_F(event_state_test, stateless_start_no_ongoing_on_triggered_callbacks) {
    START_WAIT_TRIGGERED(event_state_stateless_, cloud::utils::time::now());

    WAIT_ONGOING(event_state_stateless_, 1, false);

    EXPECT_NO_THROW();
}