#include "event-manager.h"

#include <functional>

using namespace vxg::cloud::agent;

namespace vxg {
namespace cloud {
void event_manager::__trigger_periodic_event(
    const proto::event_config& event_conf) {
    using namespace std::chrono;
    proto::event_object periodic_event;
    periodic_event.event = event_conf.event;
    periodic_event.custom_event_name = event_conf.custom_event_name;
    periodic_event.time = utils::time::to_double(utils::time::now());
    periodic_event.status = proto::ES_OK;

    // TODO: move qos-report from here
    if (event_conf.custom_event_name == "qos-report")
        periodic_event.meta = json(profile::global::instance().stats);

    _notify_event(periodic_event);

    if (event_conf.caps.periodic) {
        periodic_events_[event_conf.name()] =
            timed_cb_scheduler_->schedule_timed_cb(
                std::bind(&event_manager::__trigger_periodic_event, this,
                          event_conf),
                event_conf.period * 1000);
    }
}

void event_manager::_schedule_periodic_event(
    const proto::event_config& event_conf) {
    if (event_conf.caps.periodic && event_conf.active) {
        if (event_conf.period == 0) {
            logger->warn(
                "User tried to set period to 0 via API, treating it as "
                "disabled event!");
            return;
        }

        auto p = timed_cb_scheduler_->schedule_timed_cb(
            std::bind(&event_manager::__trigger_periodic_event, this,
                      event_conf),
            event_conf.period * 1000);
        logger->info("Starting periodic event {} period {} timer {}",
                     event_conf.name(), event_conf.period, (void*)p.get());
        if (p) {
            if (periodic_events_.count(event_conf.name()) &&
                periodic_events_[event_conf.name()])
                timed_cb_scheduler_->cancel_timed_cb(
                    periodic_events_[event_conf.name()]);
            periodic_events_[event_conf.name()] = p;
        } else
            logger->warn("Failed to start periodic event");
    }
}

void event_manager::_cancel_periodic_event(
    const proto::event_config& event_conf) {
    if (periodic_events_.count(event_conf.name())) {
        logger->info("Stopping periodic event {} timer {}", event_conf.name(),
                     (void*)periodic_events_[event_conf.name()].get());

        if (periodic_events_[event_conf.name()]) {
            timed_cb_scheduler_->cancel_timed_cb(
                periodic_events_[event_conf.name()]);
            periodic_events_[event_conf.name()] = nullptr;
        }
        periodic_events_.erase(event_conf.name());
    }
}

void event_manager::_schedule_periodic_events(proto::events_config& events) {
    for (auto& event_conf : events.events) {
        if (event_conf.caps.periodic)
            _schedule_periodic_event(event_conf);
    }
}

void event_manager::_cancel_periodic_events(
    const proto::events_config& events) {
    for (auto& event_conf : events.events) {
        _cancel_periodic_event(event_conf);
    }
}

void event_manager::_append_internal_custom_events(
    proto::events_config& config) {
    if (config_.send_qos_report_as_separate_event) {
        logger->info("Append QOS event to events config");

        std::vector<proto::event_config> conf_list;
        proto::event_config qos_event_config;

        qos_event_config.event = proto::ET_CUSTOM;
        qos_event_config.custom_event_name = "qos-report";
        qos_event_config.active = true;
        qos_event_config.period = config_.send_qos_report_period_sec;
        qos_event_config.snapshot = false;
        qos_event_config.stream = false;

        qos_event_config.caps.periodic = true;
        qos_event_config.caps.trigger = false;
        qos_event_config.caps.stream = false;
        qos_event_config.caps.snapshot = false;

        conf_list.push_back(qos_event_config);

        // Append if not yet in config, update otherwise
        if (std::find_if(config.events.begin(), config.events.end(),
                         [=](const proto::event_config& c) {
                             return qos_event_config.name_eq(c);
                         }) == config.events.end()) {
            config.events.push_back(qos_event_config);
        } else
            _update_events_configs(conf_list, config.events, false);
    }

    {
        logger->info("Append timeline-sync event to events config");

        std::vector<proto::event_config> conf_list;
        proto::event_config ts_event_config;

        ts_event_config.event = proto::ET_CUSTOM;
        ts_event_config.custom_event_name = "timeline-sync";
        ts_event_config.active = true;
        ts_event_config.snapshot = false;
        ts_event_config.stream = true;
        ts_event_config.caps.periodic = false;
        ts_event_config.caps.trigger = false;
        ts_event_config.caps.stream = true;
        ts_event_config.caps.snapshot = false;
        ts_event_config.caps.stateful = true;
        ts_event_config.caps.internal_hidden = true;

        conf_list.push_back(ts_event_config);

        // Append if not yet in config, update otherwise
        if (std::find_if(config.events.begin(), config.events.end(),
                         [=](const proto::event_config& c) {
                             return ts_event_config.name_eq(c);
                         }) == config.events.end()) {
            config.events.push_back(ts_event_config);
        } else
            _update_events_configs(conf_list, config.events, false);
    }
}

void event_manager::_init_events_states(const proto::events_config& config) {
    events_states_.clear();
    for (auto& event_conf : config.events) {
        // FIXME:
        events_states_[event_conf.name()] = event_state(
            event_conf,
            std::make_shared<event_manager::callbacks>(
                event_state_report_cb_, std::bind(&event_manager::notify_event,
                                                  this, std::placeholders::_1)),
            timed_cb_scheduler_);
    }
}

bool event_manager::_update_events_configs(
    const std::vector<proto::event_config>& new_configs,
    std::vector<proto::event_config>& dest_configs,
    bool append) {
    for (auto& new_conf : new_configs) {
        auto it = std::find_if(dest_configs.begin(), dest_configs.end(),
                               [new_conf](const proto::event_config& c) {
                                   return new_conf.name_eq(c);
                               });

        if (it != dest_configs.end()) {
            it->active = new_conf.active;

            // Can apply settings only, caps may not be changed
            if (new_conf.caps_eq(*it)) {
                // Apply settings only, don't touch caps
                // Apply new flags values with respect to caps
                it->period = new_conf.period;
                it->snapshot = new_conf.snapshot && it->snapshot;
                it->stream = new_conf.stream && it->stream;
            } else {
                logger->error(
                    "Unable to apply config for event {}, capabilities "
                    "changed.",
                    json(new_conf)["event"]);
                return false;
            }
        } else if (append) {
            // No such event config yet, adding it
            dest_configs.push_back(new_conf);
        }
    }

    return true;
}

event_stream::ptr event_manager::_lookup_event_stream(const std::string& name) {
    auto es = std::find_if(event_streams_.begin(), event_streams_.end(),
                           [name](const event_stream::ptr event_stream) {
                               return name == event_stream->name();
                           });

    if (es != event_streams_.end())
        return *es;

    return nullptr;
}

bool event_manager::_update_event_stream_configs(
    const std::string& stream_name,
    const std::vector<proto::event_config>& new_configs,
    bool append) {
    std::vector<proto::event_config> temp_configs;

    logger->debug("event_streams_configs_ size {}, has {} keys {}",
                  event_streams_configs_.size(),
                  event_streams_configs_.count(stream_name), stream_name);

    // Initialization, caps should never be changed
    if (!event_streams_configs_.count(stream_name)) {
        event_streams_configs_[stream_name] = new_configs;
        return true;
    }

    return _update_events_configs(new_configs,
                                  event_streams_configs_[stream_name], append);
}

event_stream::ptr event_manager::_lookup_event_stream_by_event(
    const proto::event_object& event) {
    for (auto& kv : event_streams_configs_) {
        auto e = std::find_if(kv.second.begin(), kv.second.end(),
                              [event](proto::event_config& ec) {
                                  return event.event == ec.event &&
                                         event.custom_event_name ==
                                             ec.custom_event_name;
                              });

        if (e != kv.second.end())
            return _lookup_event_stream(kv.first);
    }

    return nullptr;
}

void event_manager::_load_events_configs(proto::events_config& config) {
    // Always enable all events we or user code provides by default, their
    // actual state is stored in the Cloud DB and will be changed with
    // set_events()
    config.enabled = true;

    _append_internal_custom_events(config);

    // Per event stream configs, retreive events provided by all event streams
    for (auto& event_stream : event_streams_) {
        std::vector<proto::event_config> stream_events;

        // Retrieve events conf from event stream
        if (event_stream->get_events(stream_events)) {
            std::string events_str;
            for (std::vector<proto::event_config>::iterator ec =
                     stream_events.begin();
                 ec != stream_events.end(); ec++) {
                json jec = *ec;
                events_str += jec["event"];

                if (std::distance(ec, stream_events.end()) != 1)
                    events_str += ", ";
            }
            logger->info("Event stream {} provides {} events: {}",
                         event_stream->name(), stream_events.size(),
                         events_str);

            // Apply retrieved configs to event stream config holder and all
            // events configs holder
            if (!(_update_event_stream_configs(event_stream->name(),
                                               stream_events, true) &&
                  _update_events_configs(stream_events, config.events, true)))
                logger->error(
                    "Unable to apply events config for event stream {}",
                    event_stream->name());
        } else {
            logger->warn(
                "Event stream {} failed to report its events config, all "
                "events notified by this event stream will be ignored!",
                event_stream->name());
        }
    }

    if (events_states_.empty())
        _init_events_states(config);
}

void event_manager::_start_all_event_streams() {
    for (auto& s : event_streams_) {
        // Start event stream, event providers must immediately
        // notify states of all stateful events
        logger->info("Start event stream {}", s->name());
        s->set_notification_cb(std::bind(&event_manager::_notify_event, this,
                                         std::placeholders::_1));
        s->start();
    }
}

void event_manager::_stop_all_event_streams() {
    for (auto& s : event_streams_) {
        logger->info("Stop event stream {}", s->name());
        s->stop();
    }

    // Loop over all event states and stop all active stateful events
    auto now = utils::time::now();
    for (auto& state : events_states_) {
        if (state.second.stateful() && state.second.active() &&
            state.second.need_record()) {
            logger->info("Force stopping event {} at {}", state.first,
                         utils::time::to_iso(now));
            state.second.stop(now);
        }
    }
}

bool event_manager::handle_stateful_event(const proto::event_object& event) {
    using namespace std::chrono;

    if (event.active == events_states_[event.name()].active()) {
        logger->debug("Wrong {} state change {} -> {} !!!", event.name(),
                      events_states_[event.name()].active(), event.active);
        return false;
    } else if (!events_states_[event.name()].active() &&
               events_states_[event.name()].start() ==
                   utils::time::from_double(event.time)) {
        logger->debug(
            "New event {} with the same time {}, ignorring !!!", event.name(),
            utils::time::to_iso(utils::time::from_double(event.time)));
        return false;
    }

    logger->info("Event {} {} at {}", event.name(),
                 event.active ? "STARTED" : "FINISHED",
                 utils::time::to_iso(utils::time::from_double(event.time)));

    if (utils::time::from_double(event.time) == utils::time::null()) {
        logger->warn("Event {} time is wrong, skipping", event.name());
        return false;
    }

    if (event.active)
        events_states_[event.name()].start(
            utils::time::from_double(event.time));
    else if (events_states_[event.name()].active())
        events_states_[event.name()].stop(utils::time::from_double(event.time));

    return true;
}

bool event_manager::handle_stateless_event(const proto::event_object& event) {
    using namespace std::chrono;

    logger->info("Event {} TRIGGERED at {}", event.name(),
                 utils::time::to_iso(utils::time::from_double(event.time)));

    if (utils::time::from_double(event.time) == utils::time::null()) {
        logger->warn("Event {} time is wrong, skipping", event.name());
        return false;
    }

    events_states_[event.name()].start(utils::time::from_double(event.time));
    return true;
}

bool event_manager::_notify_event(const agent::proto::event_object& e) {
    proto::event_config event_config;
    /** check if event was configured in get_events() callback
     *  unknown events are ignored
     */
    if (!events_config_.get_event_config(e, event_config)) {
        logger->warn(
            "Unable to find config for event {}, did you specify "
            "agent::callback::on_get_cam_events_config() or "
            "agent::event_stream::get_events()?",
            e.name());
        return false;
    }

    logger->trace("Queueing event {}", e.name());

    async_events_handler_->push(e);
    return true;
}

void event_manager::_handle_event(proto::event_object event) {
    using namespace std::chrono;
    proto::event_config event_config;

    if (!events_config_.get_event_config(event, event_config)) {
        logger->warn(
            "Unable to find config for event {}, did you specify "
            "agent::callback::on_get_cam_events_config() or "
            "agent::event_stream::get_events()?",
            event.name());
        return;
    }

    if (event.state_emulation)
        logger->debug("Dummy event {}", event.name());

    // Check if event was disabled
    if (!event_config.active && !event.state_emulation) {
        logger->warn("Notified event {} disabled by the Cloud API",
                     event.name());
        return;
    }

    // Handle records uploading
    if (!event.state_emulation) {
        if (event_config.caps.stateful) {
            if (!handle_stateful_event(event))
                return;

            // Nothing to do on stateful event stop
            if (!event.active)
                return;
        } else
            handle_stateless_event(event);
    }

    // Event requires snapshot if it was configured correspondingly
    // Send snapshot for stateless events and for start of stateful event
    // Send snapshot if stateful_event_continuation_kick_snapshot is true and
    // this is a dummy event for event statefulness emulation
    bool need_snapshot = event_config.snapshot &&
                         (((event_config.caps.stateful && event.active) ||
                           !event_config.caps.stateful) ||
                          (config_.stateful_event_continuation_kick_snapshot &&
                           event.state_emulation));

    handle_event_payload_cb_(event, need_snapshot);
}

bool event_manager::trigger_event(const std::string& event,
                                  const json& meta,
                                  cloud::time time) {
    logger->info("Event {} triggered with time {} meta '{}'", event,
                 utils::time::to_iso(time), meta.dump());

    // Construct json representation of the triggered event
    // This is neccesary to automatically convert standard/custom name of event
    json jevent;
    jevent["event"] = event;

    // Convert json event to event object and find it's configuration and caps
    proto::event_object triggered_event = jevent;
    triggered_event.meta = meta;
    triggered_event.time = utils::time::to_double(time);

    vxg::cloud::agent::proto::event_config event_conf;
    if (events_config_.get_event_config(triggered_event, event_conf)) {
        // If event was specified as supported one and triggerable in the
        // on_get_cam_events_config() and wasn't disabled by user through the
        // Cloud API we call user code and if user returned true we notify this
        // event to the Cloud, user may adjust event's fields such as time or
        // snapshot for instance.
        if (event_conf.caps.trigger && event_conf.active) {
            auto event_stream = _lookup_event_stream_by_event(triggered_event);

            // If event provided by event_stream we trigger it via the
            // corresponding event_stream
            if (event_stream != nullptr)
                return event_stream->trigger_event(triggered_event);

            // If event provided not by the event stream but by the common
            // callback we trigger it via the common callback
            // FIXME:
            // if (callback_->on_trigger_event(triggered_event))
            //     return handle_event_payload_cb_(triggered_event);
        }
    } else {
        // FIXME: change message
        logger->error(
            "Triggered unknown event {}, custom events should be specified with"
            " the agent_callback::on_get_cam_events_config()",
            event);
    }

    return false;
}

bool event_manager::get_events(proto::events_config& config) {
    // Stop all event producers
    _cancel_periodic_events(events_config_);
    _stop_all_event_streams();

    // Load event configs and caps before applying new settings
    if (events_config_.events.empty())
        _load_events_configs(events_config_);

    config = events_config_;
    // Don't report internal events
    config.events.erase(
        std::remove_if(
            config.events.begin(), config.events.end(),
            [](proto::event_config& ec) { return ec.caps.internal_hidden; }),
        config.events.end());

    // Start events providers
    // Start periodic events internal timers
    _schedule_periodic_events(events_config_);

    if (events_config_.enabled) {
        // Start event streams if required, event providers must immediately
        // notify states of all stateful events
        _start_all_event_streams();
    } else {
        // Stop all event streams
        _stop_all_event_streams();
    }
    // TODO: move memorycard event to internal events
    // FIXME:
    // _update_storage_status();

    return true;
}

bool event_manager::set_events(const proto::events_config& config) {
    // Disable all periodic events to apply new settings
    _cancel_periodic_events(events_config_);
    _stop_all_event_streams();

    // Load events configs and caps before applying new settings
    if (events_config_.events.empty())
        _load_events_configs(events_config_);

    // Update per-event stream configs, only already presented events for the
    // event stream will be updated
    for (auto& event_stream : event_streams_) {
        _update_event_stream_configs(event_stream->name(), config.events,
                                     false);
        event_stream->set_events(event_streams_configs_[event_stream->name()]);
    }

    // Save enabled flag
    events_config_.enabled = config.enabled;
    // Apply settings only, don't touch caps
    _update_events_configs(config.events, events_config_.events, false);
    // TODO: move memorycard event to internal events
    // FIXME:
    // _update_storage_status();

    if (events_states_.empty())
        _init_events_states(events_config_);

    // Start/stop event streams according to global 'enabled' flag
    if (events_config_.enabled) {
        // Start event streams if required, event providers must immediately
        // notify states of all stateful events
        _start_all_event_streams();
    } else {
        // Stop all event streams
        _stop_all_event_streams();
    }

    // Reschedule periodic events with new settings
    _schedule_periodic_events(events_config_);

    return true;
}

void event_manager::start() {
    async_events_handler_ =
        std::make_shared<utils::queued_async_handler<proto::event_object>>(
            std::bind(&event_manager::_handle_event, this,
                      std::placeholders::_1));
    async_events_handler_->start();
}

void event_manager::stop() {
    if (async_events_handler_)
        async_events_handler_->stop();
}

bool event_manager::notify_event(const proto::event_object& event) {
    return _notify_event(event);
}

event_manager::event_manager(
    const config& config,
    std::vector<agent::event_stream::ptr> event_streams,
    event_state_report_cb_ptr report_cb,
    handle_event_payload_cb handle_event_payload_cb = nullptr)
    : event_streams_ {event_streams},
      event_state_report_cb_ {report_cb},
      handle_event_payload_cb_ {handle_event_payload_cb} {
    transport_ = std::make_shared<transport::libwebsockets::http>();
    timed_cb_scheduler_ =
        std::dynamic_pointer_cast<transport::timed_callback>(transport_);
    transport_->start();
}

event_manager::~event_manager() {
    stop();
    _cancel_periodic_events(events_config_);
    _stop_all_event_streams();
    transport_->stop();
}

}  // namespace cloud
}  // namespace vxg