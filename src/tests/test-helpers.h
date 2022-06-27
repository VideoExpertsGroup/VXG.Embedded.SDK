#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <future>

using namespace ::testing;

#define WAIT_FOR_CALL(obj, call, trigger)                                      \
    {                                                                          \
        std::promise<void> prom;                                               \
        EXPECT_CALL(obj, call()).Times(1).WillOnce(testing::Invoke([&prom]() { \
            prom.set_value();                                                  \
        }));                                                                   \
                                                                               \
        trigger;                                                               \
                                                                               \
        EXPECT_EQ(std::future_status::ready,                                   \
                  prom.get_future().wait_for(std::chrono::seconds(3)));        \
    }

#define WAIT_FOR_CALL2(obj, call, trigger)                              \
    {                                                                   \
        std::promise<void> prom;                                        \
        EXPECT_CALL(obj, call).Times(1).WillOnce(                       \
            testing::Invoke([&prom]() { prom.set_value(); }));          \
                                                                        \
        trigger;                                                        \
                                                                        \
        EXPECT_EQ(std::future_status::ready,                            \
                  prom.get_future().wait_for(std::chrono::seconds(3))); \
    }

#define WAIT_FOR_CALL3(obj, call, args, trigger)                        \
    {                                                                   \
        std::promise<void> prom;                                        \
                                                                        \
        auto invoke_cb = [&prom](auto&&... param) {                     \
            auto ret = true;                                            \
            prom.set_value();                                           \
            return ret;                                                 \
        };                                                              \
                                                                        \
        EXPECT_CALL(obj, call).Times(1).WillOnce(invoke_cb);            \
                                                                        \
        trigger;                                                        \
                                                                        \
        EXPECT_EQ(std::future_status::ready,                            \
                  prom.get_future().wait_for(std::chrono::seconds(3))); \
    }

#define WAIT_FOR_CALL_AND_DO(obj, call, trigger, act)                      \
    {                                                                      \
        std::promise<void> prom;                                           \
        EXPECT_CALL(obj, call()).Times(1).WillOnce(testing::Invoke([&]() { \
            act();                                                         \
                                                                           \
            prom.set_value();                                              \
        }));                                                               \
                                                                           \
        trigger;                                                           \
                                                                           \
        EXPECT_EQ(std::future_status::ready,                               \
                  prom.get_future().wait_for(std::chrono::seconds(3)));    \
    }
