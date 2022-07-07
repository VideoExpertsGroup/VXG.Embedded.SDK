#include "queued-async-storage.h"

#include <utils/utils.h>

TEST_F(queued_async_storage_test, create) {
    EXPECT_NO_THROW();
}

TEST_F(queued_async_storage_test, concurent_limit) {
    auto now = utils::time::now();
    auto i1 = std::make_shared<timed_storage::item>(
        period(now + std::chrono::seconds(1), now + std::chrono::seconds(2)));
    auto i2 = std::make_shared<timed_storage::item>(
        period(now + std::chrono::seconds(2), now + std::chrono::seconds(3)));
    auto i3 = std::make_shared<timed_storage::item>(
        period(now + std::chrono::seconds(3), now + std::chrono::seconds(4)));

    std::promise<void> prom;
    size_t stores = 0;
    EXPECT_CALL(_async_store, Call(_))
        .Times(Exactly(3))
        .WillRepeatedly(
            testing::Invoke([&](queued_async_storage::async_item_ptr i) {
                stores++;

                vxg::logger::info("async_store");

                if (stores == 3) {
                    return false;
                }

                i->on_finished(true);
                return true;
            }));

    size_t good = 0, fail = 0;
    timed_storage::async_store_finished_cb finished_cb = [&](bool ok) {
        good += ok;
        fail += !ok;

        vxg::logger::info("on_finished {}", ok);

        if (good + fail == 3)
            prom.set_value();
    };

    storage_->store_async(i1, finished_cb);
    storage_->store_async(i2, finished_cb);
    storage_->store_async(i3, finished_cb);

    EXPECT_EQ(std::future_status::ready,
              prom.get_future().wait_for(std::chrono::seconds(115)));

    EXPECT_EQ(good, 2U);
    EXPECT_EQ(fail, 1U);
    EXPECT_NO_THROW();
}