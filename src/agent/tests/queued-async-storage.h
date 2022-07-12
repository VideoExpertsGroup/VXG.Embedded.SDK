#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <future>

#include <agent/direct-upload-storage.h>
#include <agent/timeline.h>
#include <tests/test-helpers.h>

using namespace ::testing;
using namespace std;

using namespace vxg;
using namespace vxg::cloud;
using namespace vxg::cloud::agent;

struct queued_async_storage_test : public Test {
    queued_async_storage_ptr storage_;
    size_t max_concurent_stores_;

    MockFunction<bool(queued_async_storage::async_item_ptr i)> _async_store;
    bool _async_store_wrap(queued_async_storage::async_item_ptr i) {
        return _async_store.AsStdFunction()(i);
    }

    virtual void SetUp() override {
        max_concurent_stores_ = 0;
        storage_ = std::make_shared<queued_async_storage>(
            std::bind(&queued_async_storage_test::_async_store_wrap, this,
                      std::placeholders::_1));
        storage_->init();
    }

    virtual void TearDown() override { storage_->finit(); }
};
