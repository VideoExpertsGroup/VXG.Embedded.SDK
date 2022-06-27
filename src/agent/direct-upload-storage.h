#pragma once

#include <net/http.h>
#include <utils/logging.h>

#include <agent/event-manager.h>
#include <agent/timeline-synchronizer.h>

namespace vxg {
namespace cloud {
namespace agent {

class queued_async_storage : public timed_storage {
    vxg::logger::logger_ptr logger =
        vxg::logger::instance("queued-async-storage");

public:
    struct async_item : public item {
        async_store_finished_cb on_finished;
        async_store_is_canceled_cb is_canceled;

        async_item(
            period p,
            std::vector<uint8_t>&& data,
            async_store_finished_cb f,
            async_store_is_canceled_cb c = []() { return false; })
            : item(p, std::move(data)), on_finished {f}, is_canceled {c} {}

        async_item(item_ptr i,
                   async_store_finished_cb f,
                   async_store_is_canceled_cb c)
            : item(*i), on_finished {f}, is_canceled {c} {}
    };
    using async_item_ptr = std::shared_ptr<async_item>;
    using async_store_cb = std::function<bool(async_item_ptr)>;

private:
    std::deque<async_item_ptr> q_;
    transport::libwebsockets::http::ptr worker_scheduler_;
    transport::timed_cb_ptr worker_timer_;
    std::chrono::milliseconds worker_loop_delay_ {
        std::chrono::milliseconds(1000)};

    size_t max_concurrent_processing_items_ {2};
    size_t processing_items_ {0};
    size_t processed_items_ {0};
    size_t failed_items_ {0};
    async_store_cb async_store_cb_;

    void _worker_loop() {
        while (!q_.empty() &&
               processing_items_ < max_concurrent_processing_items_) {
            auto i = q_.front();
            q_.pop_front();

            processing_items_++;

            // Intercept original callback, invoke it after we done with our
            // stuff
            auto orig_finished_cb = i->on_finished;
            i->on_finished = [orig_finished_cb, this](bool ok) {
                processing_items_--;
                processed_items_ += ok;
                failed_items_ += !ok;

                logger->info("Async storage item {}",
                             ok ? "stored successfully" : "store failed");

                orig_finished_cb(ok);
            };

            if (!async_store_cb_(i)) {
                orig_finished_cb(false);
                processing_items_--;
                failed_items_++;
            }
        }

        worker_timer_ = worker_scheduler_->schedule_timed_cb(
            [=] { _worker_loop(); }, worker_loop_delay_.count());
    }

public:
    queued_async_storage(async_store_cb cb)
        : worker_scheduler_ {std::make_shared<
              transport::libwebsockets::http>()},
          async_store_cb_ {cb} {}
    virtual ~queued_async_storage() {}

    virtual bool init() override {
        worker_scheduler_->start();

        worker_timer_ = worker_scheduler_->schedule_timed_cb(
            [=] { _worker_loop(); }, worker_loop_delay_.count());
        return true;
    }

    virtual void finit() override {
        std::condition_variable barrier;
        std::mutex lock;
        std::unique_lock<std::mutex> ulock(lock);

        // We need to run cancellation inside the scheduled callback to
        // serialize it against _worker_loop()
        worker_scheduler_->schedule_timed_cb(
            [&] {
                {
                    std::lock_guard<std::mutex> l(lock);

                    if (worker_timer_) {
                        worker_scheduler_->cancel_timed_cb(worker_timer_);
                        worker_timer_ = nullptr;
                    }
                }
                barrier.notify_one();
            },
            0);
        barrier.wait(ulock, [&]() { return !worker_timer_; });

        worker_scheduler_->stop();
    }

    virtual std::vector<item_ptr> list(cloud::time start,
                                       cloud::time stop) override {
        std::vector<item_ptr> result;
        // FIXME:
        return result;
    }

    virtual bool load(item_ptr i) override {
        // FIXME:
        return false;
    }

    virtual bool store(item_ptr) override { return false; }

    virtual bool store_async(
        item_ptr item,
        async_store_finished_cb on_finished = nullptr,
        async_store_is_canceled_cb is_canceled = nullptr) override {
        // enqueue item inside the scheduled callback here for serialization
        worker_scheduler_->schedule_timed_cb(
            std::bind(
                [on_finished, is_canceled, this](item_ptr item) {
                    async_item_ptr i = std::make_shared<async_item>(
                        std::move(item), on_finished, is_canceled);
                    q_.push_back(i);
                },
                item),
            1);

        return true;
    }

    virtual void erase(item_ptr) {}
};
using queued_async_storage_ptr = std::shared_ptr<queued_async_storage>;

class direct_upload_proto_storage : public queued_async_storage {
    // TODO: storing cloud_storage object here just for reusing it's list()
    // method  is not good idea
    cloud_storage remote_storage_;

public:
    direct_upload_proto_storage(agent::proto::access_token token,
                                transport::libwebsockets::http::ptr http,
                                async_store_cb direct_upload_request_cb)
        : queued_async_storage(direct_upload_request_cb),
          remote_storage_ {token, http} {}
    virtual ~direct_upload_proto_storage() {}

    virtual std::vector<item_ptr> list(cloud::time start,
                                       cloud::time stop) override {
        return remote_storage_.list(start, stop);
    }
};
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
