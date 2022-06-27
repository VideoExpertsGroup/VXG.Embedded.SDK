#pragma once

#include <agent/callback.h>

struct mock_agent_callback : public vxg::cloud::agent::callback {
    mock_agent_callback() {}
    virtual ~mock_agent_callback() {}

    MOCK_METHOD(void, on_bye, (proto::command::bye_reason reason), (override));
    MOCK_METHOD(bool,
                on_get_memorycard_info,
                (proto::event_object::memorycard_info_object&),
                (override));
};
using mock_agent_callback_ptr = std::unique_ptr<mock_agent_callback>;