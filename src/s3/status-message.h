#pragma once

#include <future>

#include <net/transport.h>

class status_messages {
public:
    struct message;

private:
    vxg::cloud::transport::Queue<message> status_msg_q_;
    std::function<void(const message& message)> message_cb_;
    std::atomic<int> running_;
    std::future<bool> looper_;

public:
    using status_message_func = std::function<void(const message& message)>;
    struct message {
        std::string source;
        std::string message;
        std::string description;
    };

    status_messages(status_message_func cb = nullptr) : message_cb_ {cb} {
        looper_ = std::async(std::launch::async, [this]() {
            while (running_) {
                message m;
                if (running_ &&
                    status_msg_q_.pop(m, std::chrono::milliseconds(500)))
                    message_cb_(m);
            }
            return true;
        });
    }

    ~status_messages() {
        running_ = false;
        status_msg_q_.flush();
        looper_.wait();
    }

protected:
    void report_status(const std::string& msg_text) {
        if (message_cb_) {
            message msg;
            msg.source = typeid(*this).name();
            msg.message = msg_text;
            status_msg_q_.push(std::move(msg));
        }
    }

    status_message_func get_cb() { return message_cb_; }
};