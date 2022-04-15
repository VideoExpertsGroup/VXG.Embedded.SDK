#include "manager.h"
#include <tests/test-helpers.h>

TEST_F(agent_manager_test, Create) {
    // WAIT_FOR_CALL(*this, on_connected, this->transport_->connect(""));
    sleep(5);
    EXPECT_NO_THROW();
}