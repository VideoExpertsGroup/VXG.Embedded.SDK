#ifndef __EVENT_STREAM_H
#define __EVENT_STREAM_H

#include <vector>

#include <agent-proto/proto.h>

namespace vxg {
namespace cloud {
namespace agent {
//!
//! @brief Event stream, abstract class for event generation
//!
class event_stream {
    //! \private
    std::function<bool(proto::event_object)> notify_cb_ {nullptr};
    std::string name_;

    std::string _derived_class_name() {
        return utils::gcc_abi::demangle(typeid(*this).name());
    }

public:
    //! @brief std::shared_ptr to event_stream
    typedef std::shared_ptr<event_stream> ptr;

    //! @brief Construct a new event stream object
    //!
    //! @param[in] name Event stream name, unique name for event stream
    //!
    event_stream(std::string name) : name_ {name} {}

    virtual ~event_stream() {}

    //! \private
    void set_notification_cb(std::function<bool(proto::event_object)> cb) {
        notify_cb_ = cb;
    }

    //! @brief Callback should be called to notify event
    //!
    //! @param[in] event Event object
    //! @return true Event successfully notified
    //! @return false Notification failed
    //!
    bool notify(proto::event_object event) {
        if (notify_cb_)
            return notify_cb_(event);
        return false;
    }

    //! \private
    std::string name() { return name_; }

    //! @brief Start events generation, called by internal code when the events
    //!        generation requested by the VXG Cloud.
    //!        Event stream MUST immediately notify states of all stateful
    //!        events after the start() was invoked.
    //!
    //! @return true Events generation started
    //! @return false Failed to start events generation
    //!
    virtual bool start() = 0;
    //!
    //! @brief Stop events generation
    //!
    virtual void stop() = 0;

    //! @brief Get the events configs list
    //! This method should update @p config object and add all configurations
    //! for the events provided by this event stream. @p config may already
    //! include event configs reported by this get_event(), hence the
    //! implementation should consider this and do not include its event configs
    //! more than one time.
    //!
    //! @param[out] configs Events configurations.
    //! @return true @p configs is valid.
    //! @return false @p configs is invalid, should not be applied.
    //! @note This method MUST always return the configs with the same caps,
    //! otherwise the new config will not be applied by the library.
    virtual bool get_events(std::vector<proto::event_config>& configs) = 0;

    //! @brief Set the events configuration.
    //!
    //! @param config Events configurations list which includes all events
    //! reported by the system and other event streams, implementation should
    //! find own event configurations and apply them.
    //! @return true @p config applied.
    //! @return false @p config not applied.
    virtual bool set_events(const std::vector<proto::event_config>& config) = 0;

    //! @brief Trigger event provided by event_stream
    //! If get_events() returned event config with
    //! proto::event_config.caps.trigger == true and this event was triggered
    //! via the Cloud API this method will be called.
    //! The logic of this method should be the same as for
    //! vxg::cloud::agent::callback::on_trigger_event().
    //! @see vxg::cloud::agent::callback::on_trigger_event()
    //!
    //! @param event
    //! @return true
    //! @return false
    virtual bool trigger_event(proto::event_object& event) {
        vxg::logger::instance("event-stream")
            ->warn("{} not implemented, implement it in {}", __func__,
                   _derived_class_name());
        return false;
    }

    //! @brief Turn on/off the event_stream triggered recording and pre/post
    //!        recording time.
    //!        Triggered recording means that event
    //!        generated by this event_stream should start recording.
    //!        Final recorded file should have duration of pre time + duration
    //!        of the even + post time.
    //! @note  Trigger driven recording can be used if platform supports such
    //!        type of recording, implementation of such type of recording
    //!        should include specific agent::media::stream records exporting
    //!        mechanism which handles two consecutive events pre/post time
    //!        intersections.
    //!
    //! @param[in] enabled true if event stream should trigger the recording.
    //!            Implementation may ignore this if not trigger driven record
    //!            method is used.
    //! @param[in] pre Pre recording time in milliseconds.
    //! @param[in] post Post recording time in milliseconds.
    //! @return true
    //! @return false
    //!
    virtual bool set_trigger_recording(bool enabled, int pre, int post) = 0;

    //! @internal
    //! @brief Initialize the events generation, should be implemented in the
    //!        derived class, called by the internal code before calling
    //!        start(). Its recommended to allocate everything required by the
    //!        event stream in this function.
    //!        (NOT USED BY NOW)
    //!
    //! @return true Initialization success
    //! @return false Failed to initialize event stream
    //! @endinternal
    virtual bool init() = 0;
    //! @internal
    //! @brief Finalize event_stream, called by internals after calling stop()
    //!        (NOT USED BY NOW)
    //! @endinternal
    virtual void finit() = 0;
};
}  // namespace agent
}  // namespace cloud
}  // namespace vxg

#endif