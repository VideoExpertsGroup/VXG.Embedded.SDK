//  Copyright © 2022 VXG Inc. All rights reserved.
//  Contact: https://www.videoexpertsgroup.com/contact-vxg/
//  This file is part of the demonstration of the VXG Cloud Platform.
//
//  Commercial License Usage
//  Licensees holding valid commercial VXG licenses may use this file in
//  accordance with the commercial license agreement provided with the
//  Software or, alternatively, in accordance with the terms contained in
//  a written agreement between you and VXG Inc. For further information
//  use the contact form at https://www.videoexpertsgroup.com/contact-vxg/

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

    static std::string __get_this_tid() {
        std::stringstream ss;
        ss << std::hex << std::showbase << std::setw(8) << std::setfill('0')
           << std::nouppercase << std::this_thread::get_id();
        return ss.str();
    }

public:
    lws_common(std::function<void(transport::Data&)> handler = nullptr)
        : worker(handler) {}
    virtual ~lws_common() {}
    struct timed_cb_priv {
        struct lws_sorted_usec_list sul;
        timed_cb_ptr owner;
    };

private:
    void _cancel_timed_cb(timed_cb_ptr cb_id) {
        timed_cb_weak_ptr wp = std::weak_ptr<timed_cb>(cb_id);
        auto p = wp.lock();

        if (p && p->priv && lws_context_) {
            auto priv = static_cast<timed_cb_priv*>(p->priv);

            vxg::logger::instance("lws-common")
                ->trace("Canceling timer {} on TID:{}", (void*)p.get(),
                        __get_this_tid());
            vxg::logger::instance("lws-common")->flush();

            lws_sul_schedule(lws_context_, 0,
                             static_cast<lws_sorted_usec_list_t*>(&priv->sul),
                             (sul_cb_t)0xffffffff, LWS_SET_TIMER_USEC_CANCEL);

            delete static_cast<timed_cb_priv*>(priv);
            p->priv = nullptr;
        }
    }

// Explicitly disable offsetof warning as usage is valid here
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
    static void _sul_cb(struct lws_sorted_usec_list* sul) {
        struct timed_cb_priv* t =
            lws_container_of(sul, struct timed_cb_priv, sul);
        vxg::logger::instance("lws-common")
            ->trace("Callbacking timer {} on TID:{}", (void*)t->owner.get(),
                    __get_this_tid());
        vxg::logger::instance("lws-common")->flush();

        if (t->owner) {
            timed_cb_weak_ptr wp = std::weak_ptr<timed_cb>(t->owner);
            if (auto p = wp.lock()) {
                if (p->cb)
                    p->cb();

                if (p->priv) {
                    // This will unref the owner so we should check if it was
                    // freed
                    delete static_cast<timed_cb_priv*>(p->priv);
                    p->priv = nullptr;
                }
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
        auto priv = new timed_cb_priv();

        // This also refs t, its necessary to prevent deletion of the t right
        // after we returned from this function if nobody outside refed it.
        // It will be unrefed in _sul_cb while deletion of the priv or in
        // cancel_*()
        priv->owner = t;
        t->priv = priv;

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