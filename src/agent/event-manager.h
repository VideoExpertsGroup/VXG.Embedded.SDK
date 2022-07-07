#pragma once

#include <agent/event-state.h>
#include <agent/event-stream.h>
#include <agent/timeline-synchronizer.h>
#include <utils/queued-handler.h>

namespace vxg {
namespace cloud {
namespace agent {
class event_manager {
    vxg::logger::logger_ptr logger = vxg::logger::instance("event-manager");

public:
    struct config {
        //! @brief Attach qos report as motion event's meta
        bool attach_qos_report_to_motion {false};
        //! @brief Periodically send qos-report event instead of attaching qos
        //! to motion event
        bool send_qos_report_as_separate_event {false};
        //! @brief Period between the qos-report events in seconds
        size_t send_qos_report_period_sec {600};
        //! @brief Attach snapshot to event's state emulation dummy event.
        //!        Stateful events emulation kicks Cloud with event of same type
        //!        every 10 seconds during stateful event state is active. This
        //!        flag enables snapshots for such events.
        //!        Snapshot will be attached only if original event has
        //!        snapshot flag enabled in its caps and settings.
        bool stateful_event_continuation_kick_snapshot {true};
    };

    struct event_state_report_cb {
        event_state_report_cb() {}
        virtual ~event_state_report_cb() {}

        virtual void on_event_start(const event_state& state,
                                    const cloud::time& start) {}

        virtual void on_event_stop(const event_state& state,
                                   const cloud::time& stop) {}

        virtual void on_event_trigger(const event_state& state,
                                      const cloud::time& t) {}

        virtual void on_event_continue(const event_state& state,
                                       const cloud::time& t) {}

        virtual std::shared_ptr<void> on_need_stream_sync_start(
            const event_state& state,
            const cloud::time& start) {
            return nullptr;
        }

        virtual void on_need_stream_sync_stop(const event_state& state,
                                              const cloud::time& stop,
                                              std::shared_ptr<void> userdata) {}
        virtual std::shared_ptr<void> on_need_stream_sync_continue(
            const event_state& state,
            const cloud::time& t,
            std::shared_ptr<void> userdata) {
            return nullptr;
        }
    };
    using event_state_report_cb_ptr =
        std::shared_ptr<event_manager::event_state_report_cb>;
    using handle_event_payload_cb =
        std::function<bool(agent::proto::event_object&, bool)>;

private:
    struct callbacks : public event_state::event_state_changed_cb {
        event_state_report_cb_ptr event_state_report_cb_;
        using event_notify_cb = std::function<bool(proto::event_object)>;
        event_notify_cb notify_event_;
        std::map<std::string, std::shared_ptr<void>> sync_requests_userdata_;

        callbacks(event_manager::event_state_report_cb_ptr sync_cb,
                  event_notify_cb notify_cb)
            : event_state_report_cb_ {sync_cb}, notify_event_ {notify_cb} {}
        virtual ~callbacks() {}

    private:
        proto::event_object _state_emulation_event_from_config(
            const proto::event_config& config,
            const cloud::time& t) {
            proto::event_object event;
            event.event = config.event;
            event.custom_event_name = config.custom_event_name;
            event.time = utils::time::to_double(t);
            event.state_emulation = true;
            return event;
        }

    public:
        virtual void on_started(const event_state& state,
                                const cloud::time& t) override {
            event_state_report_cb_->on_event_start(state, t);

            if (state.config().caps.stream && state.config().stream) {
                sync_requests_userdata_[state.config().name()] =
                    event_state_report_cb_->on_need_stream_sync_start(state, t);
            }
        }

        virtual void on_stopped(const event_state& state,
                                const cloud::time& t) override {
            event_state_report_cb_->on_event_stop(state, t);

            if (state.config().caps.stream && state.config().stream)
                event_state_report_cb_->on_need_stream_sync_stop(
                    state, t, sync_requests_userdata_[state.config().name()]);
        }

        virtual void on_ongoing(const event_state& state,
                                const cloud::time& t) override {
            if (state.config().caps.stateful) {
                event_state_report_cb_->on_event_continue(state, t);

                if (state.config().caps.state_emulation)
                    notify_event_(
                        _state_emulation_event_from_config(state.config(), t));

                if (state.config().caps.stream && state.config().stream) {
                    // on_need_stream_sync_continue() always returns userdata
                    // If passed userdata not equal to returned one it means
                    // that the sync mode was changed
                    sync_requests_userdata_[state.config().name()] =
                        event_state_report_cb_->on_need_stream_sync_continue(
                            state, t,
                            sync_requests_userdata_[state.config().name()]);
                }
            }
        }

        virtual void on_triggered(const event_state& state,
                                  const cloud::time& t) override {
            event_state_report_cb_->on_event_trigger(state, t);

            if (state.config().caps.stream && state.config().stream) {
                auto r =
                    event_state_report_cb_->on_need_stream_sync_start(state, t);
                event_state_report_cb_->on_need_stream_sync_stop(state, t, r);
            }
        }
    };

    // map[event name, event state]
    std::map<std::string, event_state> events_states_;
    // map[event stream name, event configs list]
    std::map<std::string, std::vector<agent::proto::event_config>>
        event_streams_configs_;
    // map[event name, timed_cb_ptr]
    std::map<std::string, transport::timed_cb_ptr> periodic_events_;
    agent::proto::events_config events_config_;

    std::vector<agent::event_stream::ptr> event_streams_;
    transport::libwebsockets::http::ptr transport_;
    transport::timed_callback_ptr timed_cb_scheduler_;
    utils::queued_async_handler_ptr<agent::proto::event_object>
        async_events_handler_;
    event_state_report_cb_ptr event_state_report_cb_;
    handle_event_payload_cb handle_event_payload_cb_;
    event_manager::config config_;

    void __trigger_periodic_event(const agent::proto::event_config& event_conf);
    void _schedule_periodic_event(const agent::proto::event_config& event_conf);
    void _cancel_periodic_event(const agent::proto::event_config& event_conf);
    void _schedule_periodic_events(agent::proto::events_config& events);
    void _cancel_periodic_events(const agent::proto::events_config& events);
    void _append_internal_custom_events(agent::proto::events_config& config);
    void _init_events_states(const agent::proto::events_config& config);
    bool _update_events_configs(
        const std::vector<agent::proto::event_config>& new_configs,
        std::vector<agent::proto::event_config>& dest_configs,
        bool);
    agent::event_stream::ptr _lookup_event_stream(const std::string& name);
    bool _update_event_stream_configs(
        const std::string& stream_name,
        const std::vector<agent::proto::event_config>& new_configs,
        bool);
    agent::event_stream::ptr _lookup_event_stream_by_event(
        const agent::proto::event_object& event);
    void _load_events_configs(agent::proto::events_config& config);

    void _start_all_event_streams();
    void _stop_all_event_streams();
    bool handle_stateful_event(const agent::proto::event_object& event);
    bool handle_stateless_event(const agent::proto::event_object& event);
    bool handle_event_snapshot(agent::proto::event_object& event);
    bool handle_event_meta_file(agent::proto::event_object& event);

    bool _notify_event(const agent::proto::event_object& e);
    void _handle_event(agent::proto::event_object e);

public:
    event_manager(const event_manager::config& config,
                  std::vector<agent::event_stream::ptr> event_streams,
                  event_state_report_cb_ptr sync_cb_ptr,
                  handle_event_payload_cb);
    ~event_manager();

    void start();
    void stop();
    bool set_events(const agent::proto::events_config& config);
    bool get_events(agent::proto::events_config& config);
    bool notify_event(const agent::proto::event_object& event);
    bool trigger_event(const std::string& event,
                       const json& meta,
                       cloud::time time);
};
using event_manager_ptr = std::shared_ptr<event_manager>;
}  // namespace agent
}  // namespace cloud
}  // namespace vxg