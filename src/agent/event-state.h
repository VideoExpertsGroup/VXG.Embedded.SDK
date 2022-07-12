#pragma once

#include <agent-proto/proto.h>
#include <agent/timeline-synchronizer.h>

namespace vxg {
namespace cloud {

namespace agent {

class event_state {
    vxg::logger::logger_ptr logger = vxg::logger::instance("event-state");

    cloud::time start_ {utils::time::null()};
    cloud::time stop_ {utils::time::max()};

    bool active_ {false};
    cloud::time event_kick_time_;
    agent::proto::event_config event_conf_;
    const int MAX_EVENT_KICK_INTERVAL = 10;
    cloud::duration event_kick_period_ {
        std::chrono::seconds(MAX_EVENT_KICK_INTERVAL)};

    transport::timed_cb_ptr stateful_periodic_kicker_;
    transport::timed_callback_ptr timed_callback_provider_ {nullptr};
    std::mutex lock_kicker_;

public:
    enum stream_delivery_mode { SDM_NONE, SDM_UPLOAD, SDM_STREAM };
    struct event_state_changed_cb {
        event_state_changed_cb() {}
        virtual ~event_state_changed_cb() {}

        // Stateful event callbacks
        virtual void on_started(const event_state& state, const cloud::time&) {}
        virtual void on_stopped(const event_state& state, const cloud::time&) {}
        virtual void on_ongoing(const event_state& state, const cloud::time&) {}
        // Stateless event callback
        virtual void on_triggered(const event_state& state,
                                  const cloud::time&) {}
    };
    using event_state_changed_cb_ptr = std::shared_ptr<event_state_changed_cb>;

private:
    event_state_changed_cb_ptr state_changed_cb_;

private:
    ///! @internal
    ///! Stateful events emulation
    ///! Sending dummy event that follows up original event state change
    ///! every event_kick_period_ seconds until event state changed to
    ///! false.
    void _event_kick() {
        std::lock_guard<std::mutex> lock_(lock_kicker_);

        // Adjust event time by constant period
        event_kick_time_ += event_kick_period_;
        // Schedule next kick
        stateful_periodic_kicker_ = timed_callback_provider_->schedule_timed_cb(
            std::bind(&event_state::_event_kick, this),
            std::chrono::duration_cast<std::chrono::milliseconds>(
                event_kick_period_)
                .count());

        logger->trace("Event {} state emulation kicker {} shedulled in {} ms",
                      config().name(), (void*)stateful_periodic_kicker_.get(),
                      std::chrono::duration_cast<std::chrono::milliseconds>(
                          event_kick_period_)
                          .count());

        state_changed_cb_->on_ongoing(*this, event_kick_time_);
    }

    void _start_periodic_event_kicker() {
        event_kick_time_ = start_;
        // Schedule first kick
        stateful_periodic_kicker_ = timed_callback_provider_->schedule_timed_cb(
            std::bind(&event_state::_event_kick, this),
            std::chrono::duration_cast<std::chrono::milliseconds>(
                event_kick_period_)
                .count());
        logger->trace("Event state emulation kicker {} started",
                      (void*)stateful_periodic_kicker_.get());
    }

    void _stop_periodic_event_kicker() {
        std::lock_guard<std::mutex> lock_(lock_kicker_);

        if (stateful_periodic_kicker_) {
            logger->trace("Event state emulation kicker {} stopping",
                          (void*)stateful_periodic_kicker_.get());
            timed_callback_provider_->cancel_timed_cb(
                stateful_periodic_kicker_);
        }
    }
    ///! @endinternal
public:
    event_state() {}
    event_state(const agent::proto::event_config& event_conf,
                event_state_changed_cb_ptr state_changed_cb,
                transport::timed_callback_ptr timed_cb)
        : event_conf_ {event_conf},
          state_changed_cb_ {state_changed_cb},
          timed_callback_provider_ {timed_cb},
          event_kick_period_ {event_conf.caps.state_emulation_report_delay} {}

    ~event_state() {
        if (event_conf_.event == agent::proto::ET_INVALID)
            return;

        logger->info("Event state {} destroying", json(event_conf_)["event"]);

        // Stop periodic event kicker before destructing
        if (event_conf_.caps.stateful)
            _stop_periodic_event_kicker();

        if (start_ != utils::time::null() && event_conf_.caps.stateful &&
            stop_ == utils::time::null()) {
            stop_ = utils::time::now();

            logger->info("Event {} stateful ongoing event force stopped at {}",
                         json(event_conf_)["event"],
                         utils::time::to_iso(stop_));

            state_changed_cb_->on_stopped(*this, stop_);
        }
    }

    event_state(const event_state& r) {
        state_changed_cb_ = r.state_changed_cb_;
        timed_callback_provider_ = r.timed_callback_provider_;
        event_conf_ = r.event_conf_;
        event_kick_period_ = r.event_kick_period_;
    }

    friend void swap(event_state& l, event_state& r) {
        std::swap(l.stateful_periodic_kicker_, r.stateful_periodic_kicker_);
        std::swap(l.timed_callback_provider_, r.timed_callback_provider_);
        std::swap(l.state_changed_cb_, r.state_changed_cb_);
        std::swap(l.event_conf_, r.event_conf_);
        l.event_kick_period_ = r.event_kick_period_;
    }

    // Arg passed by value, i.e. copy constructor called first for following
    // copy-swap idiom
    event_state& operator=(event_state r) noexcept {
        // friend swap implementation
        swap(*this, r);
        return *this;
    }

    void start(cloud::time start, cloud::time stop = utils::time::null()) {
        // Statefull event
        if (event_conf_.caps.stateful) {
            active_ = true;
            start_ = start;
            stop_ = stop;

            logger->debug("Event started at {}",
                          utils::time::to_iso_packed(start_));

            // Trace event state start by owner
            state_changed_cb_->on_started(*this, start_);

            // If event stop known at start this is a not realtime event and
            // we don't need to start ongoing callback notification timer
            // but we must notify on_stopped here too
            if (stop_ != utils::time::null())
                state_changed_cb_->on_stopped(*this, stop_);
            else
                _start_periodic_event_kicker();
        } else {
            // Stateless event
            start_ = start;
            stop_ = stop;

            // Just notify event triggered
            state_changed_cb_->on_triggered(*this, start_);
        }
    }

    void stop(cloud::time time) {
        if (event_conf_.caps.stateful && stop_ == utils::time::null()) {
            _stop_periodic_event_kicker();

            stop_ = time;
            active_ = false;

            state_changed_cb_->on_stopped(*this, time);
        }
    }

    bool active() const { return active_; }
    bool stateful() const { return event_conf_.caps.stateful; }
    bool need_record() const { return event_conf_.caps.stream; }
    cloud::time start() const { return start_; }
    cloud::time stop() const { return stop_; }
    const agent::proto::event_config& config() const { return event_conf_; }
};
using event_state_ptr = std::shared_ptr<event_state>;
}  // namespace agent
}  // namespace cloud
}  // namespace vxg