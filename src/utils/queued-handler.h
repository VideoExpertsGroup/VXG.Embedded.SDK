#pragma once

#include <net/transport.h>
#include <functional>
#include <thread>

namespace vxg {
namespace cloud {
namespace utils {
template <class T>
class queued_async_handler {
public:
    using handler_func = std::function<void(const T& o)>;

private:
    vxg::cloud::transport::Queue<T> q_;
    std::function<void(const T& o)> handler_;
    std::atomic<bool> running_;
    std::thread looper_;
    std::mutex lock_;

public:
    queued_async_handler(handler_func cb = nullptr) : handler_ {cb} {}
    ~queued_async_handler() { stop(); }

    void start() {
        std::lock_guard<std::mutex> lock(lock_);
        running_ = true;
        looper_ = std::thread([this]() {
            while (running_) {
                T o;
                if (running_ && q_.pop(o, std::chrono::milliseconds(500)))
                    handler_(o);
            }
            return true;
        });
    }

    void stop() {
        std::lock_guard<std::mutex> lock(lock_);
        running_ = false;
        q_.flush();
        if (looper_.joinable()) {
            looper_.join();
        }
    }

    void push(T o) {
        if (handler_) {
            q_.push(std::move(o));
        }
    }

    handler_func get_handler() { return handler_; }
    void set_handler(handler_func h) {
        if (!running_)
            handler_ = h;
    }
};
template <class T>
using queued_async_handler_ptr = std::shared_ptr<queued_async_handler<T>>;
}  // namespace utils
}  // namespace cloud
}  // namespace vxg