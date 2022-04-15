#ifndef __TRANSPORT_H
#define __TRANSPORT_H

#include <condition_variable>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include <utils/logging.h>
#include <nlohmann/json.hpp>

#include <utils/utils.h>

using json = nlohmann::json;

namespace vxg {
namespace cloud {
namespace transport {

struct timed_cb {
    void* priv;
    std::function<void()> cb;
};
typedef std::shared_ptr<timed_cb> timed_cb_ptr;
typedef std::weak_ptr<timed_cb> timed_cb_weak_ptr;

template <class T>
class Queue {
public:
    void push(T&& t) {
        {
            // scoped lock guard, unlocked after this scope ends
            std::lock_guard<std::mutex> lock(lock_);
            q_.push_back(std::move(t));
        }

        cond_.notify_one();
    }

    bool pop(T& t, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(lock_);

        if (!cond_.wait_until(lock, std::chrono::system_clock::now() + timeout,
                              [this] { return !q_.empty(); }))
            return false;

        t = std::move(q_.front());
        q_.pop_front();

        return true;
    }

    void flush() {
        std::lock_guard<std::mutex> lock(lock_);
        q_.clear();
        cond_.notify_all();
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(lock_);
        return q_.size();
    }

    void drain(std::function<void(T&)> f) {
        std::lock_guard<std::mutex> lock(lock_);
        vxg::logger::debug("Draining {} messages", q_.size());
        while (q_.size()) {
            f(q_.front());
            q_.pop_front();
        }
    }

private:
    std::deque<T> q_;
    mutable std::mutex lock_;
    std::condition_variable cond_;
};

struct Data : public std::string {
    Data() {}

    Data(char* doc) : std::string(doc) {}

    Data(std::string doc) : std::string(doc) {}

    Data(json doc) : std::string(doc.dump()) {}
};

struct Message : public json {
    typedef std::unique_ptr<Message> U;
    typedef std::shared_ptr<Message> S;
    typedef std::weak_ptr<Message> W;
    typedef Message MessageJSON;

    Message() : json({}) {}

    Message(Data& data) : json(json::parse(data)) {}

    Message(char* doc) : json(json::parse(doc)) {}

    Message(std::string doc) : json(json::parse(doc)) {}

    Message(json doc) : json(doc) {}

    operator std::string() const { return dump(); }

    operator Data() const { return dump(); }
};

struct timed_callback {
    virtual ~timed_callback() {}

    virtual timed_cb_ptr schedule_timed_cb(std::function<void()> cb,
                                           size_t ms = 0) = 0;
    virtual void cancel_timed_cb(timed_cb_ptr t) = 0;
};
class worker : public timed_callback {
    vxg::logger::logger_ptr logger {vxg::logger::instance("transport")};

public:
    using ptr = std::shared_ptr<worker>;

    worker() : running_(false) {}
    worker(std::function<void(transport::Data&)> handler)
        : running_(false), handle_(handler) {}

    virtual ~worker() { term(); }

    virtual bool run() {
        running_ = true;

        thr_ = std::thread(&worker::poll_, this);
        // start separate rx thread and use rx queue for received messages
        if (handle_)
            rx_thr_ = std::thread(&worker::rx_handle_, this);

        return running_;
    }

    virtual void term() {
        running_ = false;

        rx_q_.flush();
        // tx_q_.clear();

        if (thr_.get_id() != std::this_thread::get_id() && thr_.joinable())
            thr_.join();
        else if (thr_.get_id() == std::this_thread::get_id() && thr_.joinable())
            thr_.detach();

        if (rx_thr_.get_id() != std::this_thread::get_id() &&
            rx_thr_.joinable())
            rx_thr_.join();
        else if (rx_thr_.get_id() == std::this_thread::get_id() &&
                 rx_thr_.joinable())
            rx_thr_.detach();
    };

    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool connect(std::string uri) { return true; }
    virtual bool disconnect() { return true; }
    virtual bool is_secure() { return false; }

    virtual bool send(const Data& message) {
        const std::lock_guard<std::mutex> lock(tx_lock_);

        tx_q_.push(message);

        return true;
    }

    virtual void* rx_handle_() {
        utils::set_thread_name("transport-rx");

        logger->info("Started");

        while (running_) {
            Data data;
            if (rx_q_.pop(data, std::chrono::milliseconds(500)) && handle_) {
                logger->debug("RX handling data, queue size {}", rx_q_.size());
                handle_(data);
                logger->debug("RX data handled");
            }

            if (rx_task_q_.size()) {
                Task t;
                rx_task_q_.pop(t, std::chrono::milliseconds(0));
                std::forward<Task>(t)();
            }
        }

        logger->info("Finished");

        return nullptr;
    }

    virtual void drain_rx() {
        run_on_rx_thread(std::bind(&Queue<Data>::drain, &rx_q_, handle_));
    }

    using Task = std::function<void()>;
    virtual void run_on_rx_thread(Task t) { rx_task_q_.push(std::move(t)); }

protected:
    // service loop
    // send message if there is one in tx_q_
    // put message in rx_q_ if received
    virtual void* poll_() = 0;

    std::thread thr_;
    std::thread rx_thr_;
    bool running_;
    Queue<Data> rx_q_;
    Queue<Task> rx_task_q_;
    std::queue<Data> tx_q_;
    std::mutex rx_lock_;
    std::mutex tx_lock_;
    std::function<void(transport::Data&)> handle_;
};

}  // namespace transport
}  // namespace cloud
}  // namespace vxg

#endif