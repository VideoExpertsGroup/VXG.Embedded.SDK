#pragma once

#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <memory>

#include <libwebsockets.h>

#include "transport.h"

namespace vxg {
namespace cloud {
namespace transport {
namespace libwebsockets {

class lws_common : public worker {
protected:
    struct lws_context* lws_context_ {nullptr};
    struct lws* client_wsi_ {nullptr};
    lws_retry_bo_t lws_retry_;
    std::mutex lws_timers_queue_lock_;
    std::vector<timed_cb_ptr> scheduled_callbacks_;

    static std::string __get_this_tid() {
        std::stringstream ss;
        ss << std::hex << std::showbase << std::setw(8) << std::setfill('0')
           << std::nouppercase << std::this_thread::get_id();
        return ss.str();
    }

public:
    lws_common(std::function<void(transport::Data&)> handler = nullptr)
        : worker(handler) {}

    virtual ~lws_common() { stop(); }

    virtual bool stop() override {
        std::lock_guard<std::mutex> _lock(lws_timers_queue_lock_);
        for (auto& cb : scheduled_callbacks_)
            _cancel_timed_cb(cb);
        _cleanup();
        return true;
    }

    struct timed_cb_priv {
        struct lws_sorted_usec_list sul;
        timed_cb_weak_ptr owner;
    };

private:
    void _cleanup() {
        if (!scheduled_callbacks_.empty())
            scheduled_callbacks_.erase(
                std::remove_if(
                    scheduled_callbacks_.begin(), scheduled_callbacks_.end(),
                    [](timed_cb_ptr t) {
                        if (t->state != timed_cb::PENDING) {
                            vxg::logger::instance("lws-common")
                                ->trace("Cleanup timer {} state {}",
                                        (void*)(t.get()),
                                        (t->state == timed_cb::TRIGGERED
                                             ? "TRIGGERED"
                                             : "CANCELED"));
                            vxg::logger::instance("lws-common")->flush();
                        }
                        return (t->state != timed_cb::PENDING);
                    }),
                scheduled_callbacks_.end());
    }

    void _cancel_timed_cb(timed_cb_ptr cb_id) {
        if (cb_id && cb_id->priv && cb_id->state == timed_cb::PENDING &&
            lws_context_) {
            auto priv = std::static_pointer_cast<timed_cb_priv>(cb_id->priv);

            vxg::logger::instance("lws-common")
                ->trace("Canceling timer {} on TID:{}", (void*)cb_id.get(),
                        __get_this_tid());
            vxg::logger::instance("lws-common")->flush();

            lws_sul_schedule(lws_context_, 0,
                             static_cast<lws_sorted_usec_list_t*>(&priv->sul),
                             (sul_cb_t)0xffffffff, LWS_SET_TIMER_USEC_CANCEL);

            cb_id->state = timed_cb::CANCELED;
        }
    }

// Explicitly disable offsetof warning as usage is valid here
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
    static void _sul_cb(struct lws_sorted_usec_list* sul) {
        struct timed_cb_priv* t =
            lws_container_of(sul, struct timed_cb_priv, sul);
        if (auto w = t->owner.lock()) {
            if (w && w->state == timed_cb::PENDING) {
                vxg::logger::instance("lws-common")
                    ->trace("Calling timed callback {} on TID:{}",
                            (void*)w.get(), __get_this_tid());
                vxg::logger::instance("lws-common")->flush();
                if (w->cb)
                    w->cb();
                // w->state may be changed in cb()
                if (w->state == timed_cb::PENDING)
                    w->state = timed_cb::TRIGGERED;
            }
        }
    }
#pragma GCC diagnostic pop
public:
    virtual std::shared_ptr<timed_cb> schedule_timed_cb(
        std::function<void()> cb,
        size_t ms) {
        std::lock_guard<std::mutex> _lock(lws_timers_queue_lock_);
        auto t = std::make_shared<timed_cb>();
        auto priv = std::make_shared<timed_cb_priv>();

        // Cleanup all canceled or triggered timers
        _cleanup();

        // This also refs t
        priv->owner = t;
        t->priv = priv;
        t->state = timed_cb::PENDING;
        scheduled_callbacks_.push_back(t);

        // Handle callback in 2 stages, we pass sul_cb_t as callback to lws and
        // call real callback inside this wrapper callback
        t->cb = cb;

        // LWSSULLI_WAKE_IF_SUSPENDED flag used to wakeup device if it went to
        // powersafe mode
        if (lws_context_) {
            vxg::logger::instance("lws-common")
                ->trace("Scheduling timer {} on TID:{}", (void*)t.get(),
                        __get_this_tid());
            vxg::logger::instance("lws-common")->flush();

            lws_sul_schedule(lws_context_, 0,
                             static_cast<lws_sorted_usec_list_t*>(&priv->sul),
                             _sul_cb, ms * LWS_US_PER_MS);
        } else
            t = nullptr;

        return t;
    }

    //! @brief Cancel scheduled timed callback.
    //!
    //! @param cb_id Timed callback
    virtual void cancel_timed_cb(timed_cb_ptr cb_id) {
        if (cb_id && running_) {
            // If we are canceling from the timed callback itself we don't need
            // to serialize anything, just call cancellation code directly
            if (std::this_thread::get_id() == this->thr_.get_id()) {
                _cancel_timed_cb(cb_id);
            } else {
                std::lock_guard<std::mutex> _lock(lws_timers_queue_lock_);
                _cancel_timed_cb(cb_id);
            }
        }
    }
};
}  // namespace libwebsockets
}  // namespace transport
}  // namespace cloud
}  // namespace vxg