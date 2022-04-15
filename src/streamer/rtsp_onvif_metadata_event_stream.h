#ifndef __RTSP_ONVIF_EVENT_STREAM_H
#define __RTSP_ONVIF_EVENT_STREAM_H

#include <agent/event-stream.h>
#include <streamer/rtsp_source.h>
#include <streamer/stream.h>

#include "onvif_metadata_sink.h"

namespace vxg {
namespace media {
namespace onvif {

class rtsp_metadata_source : public vxg::media::rtsp_source {
    static std::vector<vxg::media::Streamer::MediaType> _filter_medias() {
        std::vector<vxg::media::Streamer::MediaType> media_types;

        // Ask RTSP server for data track only
        media_types.push_back(vxg::media::Streamer::DATA);

        return media_types;
    }

public:
    rtsp_metadata_source()
        : rtsp_source(std::string("tcp"), _filter_medias()) {}

    virtual std::string name() override { return "rtsp-metadata-source"; }
};

class rtsp_onvif_stream : public vxg::media::stream {
public:
    typedef std::shared_ptr<rtsp_onvif_stream> ptr;
    rtsp_onvif_stream(
        std::function<void(const onvif::event&)> onvif_event_cb,
        std::function<void(vxg::media::Streamer::StreamError)> onvif_error_cb =
            [](vxg::media::Streamer::StreamError error) {
                vxg::logger::warn("Onvif metadata stream error");
            })
        : vxg::media::stream(
              "onvif-metadata-stream",
              std::make_shared<rtsp_metadata_source>(),
              std::make_shared<onvif::metadata_sink>(onvif_event_cb,
                                                     onvif_error_cb)) {}
};

using proto_event_parse_cb =
    std::function<bool(vxg::cloud::agent::proto::event_object& event,
                       const onvif::event& onvif_event)>;
struct onvif_topic_to_vxg_event_map {
    std::string onvif_topic;
    std::chrono::seconds min_interval {std::chrono::seconds(0)};
    std::string onvif_state_data_item;
    std::string onvif_state_data_item_active_value;
    vxg::cloud::agent::proto::event_config vxg_event_config;
    proto_event_parse_cb ext_proto_event_parse_cb;
};

class event_stream : public vxg::cloud::agent::event_stream {
    vxg::logger::logger_ptr logger {
        vxg::logger::instance("onvif-event-stream")};
    rtsp_onvif_stream::ptr event_stream_;
    std::string onvif_rtsp_url_;
    std::chrono::milliseconds pre_ {0};
    std::chrono::milliseconds post_ {0};
    bool trigger_ {false};
    bool motion_active_ {false};
    std::map<std::string, onvif_topic_to_vxg_event_map> events_map_;
    std::map<std::string, vxg::cloud::time> last_onvif_event_time_;

public:
    event_stream(std::string onvif_rtsp_url,
                 std::map<std::string, onvif_topic_to_vxg_event_map> events_map)
        : vxg::cloud::agent::event_stream("onvif-event-stream"),
          onvif_rtsp_url_ {onvif_rtsp_url},
          events_map_ {events_map} {}

    virtual bool start() override {
        init();

        if (event_stream_ && event_stream_->start(onvif_rtsp_url_) &&
            event_stream_->start_sink()) {
            logger->info("Onvif event stream from {} successfully started!",
                         onvif_rtsp_url_);
            return true;
        }

        logger->info("Onvif event stream from {} start failed!",
                     onvif_rtsp_url_);
        return false;
    }

    virtual void stop() override {
        if (event_stream_) {
            event_stream_->stop_sink();
            event_stream_->stop();
        }

        finit();

        logger->info("Onvif event stream from {} stopped!", onvif_rtsp_url_);
    }

    virtual bool get_events(std::vector<vxg::cloud::agent::proto::event_config>&
                                events_configs) override {
        for (auto& e : events_map_)
            events_configs.push_back(e.second.vxg_event_config);

        return true;
    }

    virtual bool set_events(
        const std::vector<vxg::cloud::agent::proto::event_config>&
            events_configs) override {
        for (auto& ec : events_configs) {
            // Converts event type + custom name to event name
            json j = ec;
            auto vxg_event_name = j["event"];

            // If we provide such VXG event
            if (events_map_.count(vxg_event_name)) {
                // Update VXG event config
                events_map_[vxg_event_name].vxg_event_config = ec;
            }
        }

        return true;
    }

    bool _find_mapping_by_topic(const std::string& topic,
                                onvif_topic_to_vxg_event_map& mapping) {
        // First try to find mapping with exact topic name
        auto it = std::find_if(
            events_map_.begin(), events_map_.end(),
            [topic](
                const std::pair<std::string, onvif_topic_to_vxg_event_map>& m) {
                return (topic == m.second.onvif_topic);
            });

        if (it != events_map_.end()) {
            mapping = it->second;
            return true;
        }

        // Last resort is '*' which covers all topics
        it = std::find_if(
            events_map_.begin(), events_map_.end(),
            [topic](
                const std::pair<std::string, onvif_topic_to_vxg_event_map>& m) {
                return (m.second.onvif_topic == "*");
            });

        if (it != events_map_.end()) {
            mapping = it->second;
            return true;
        }

        return false;
    }

    void on_onvif_event(const onvif::event& onvif_event) {
        using namespace vxg::cloud::agent;
        using namespace vxg::cloud;

        logger->debug("ONVIF event: {} time: {} prop_op: {}", onvif_event.topic,
                      utils::time::to_iso(onvif_event.time),
                      onvif_event.prop_op);

        if (!onvif_event.topic.empty() &&
            onvif_event.time != utils::time::null()) {
            proto::event_object event;
            std::string topic;
            onvif_topic_to_vxg_event_map mapping;

            if (_find_mapping_by_topic(onvif_event.topic, mapping)) {
                event.time = utils::time::to_double(onvif_event.time);
                event.event = mapping.vxg_event_config.event;
                event.custom_event_name =
                    mapping.vxg_event_config.custom_event_name;

                event.meta.update({{"onvif-event-topic", onvif_event.topic}});

                logger->trace("Source items:");
                for (auto& s : onvif_event.source_items) {
                    logger->trace("\tname {} value: {}", s.name, s.value);
                    event.meta.update({{s.name, s.value}});
                }

                logger->trace("Data items:");
                for (auto& d : onvif_event.data_items) {
                    logger->trace("\tname {} value: {}", d.name, d.value);
                    event.meta.update({{d.name, d.value}});
                    event.active =
                        (d.name == mapping.onvif_state_data_item &&
                         mapping.onvif_state_data_item_active_value == d.value);
                }

                event.meta_file = onvif_event.raw_body;

                // If mapping contains min interval and event is active we check
                // drop event if less than allowed time elapsed
                if (event.active) {
                    if (mapping.min_interval != std::chrono::seconds(0) &&
                        (onvif_event.time -
                         last_onvif_event_time_[onvif_event.topic]) <
                            mapping.min_interval) {
                        logger->debug(
                            "Dropping ONVIF event happened in {} seconds since "
                            "previous, min interval is {}s",
                            (double)std::chrono::duration_cast<
                                std::chrono::milliseconds>(
                                onvif_event.time -
                                last_onvif_event_time_[onvif_event.topic])
                                    .count() /
                                1000,
                            mapping.min_interval.count());
                        return;
                    }
                    last_onvif_event_time_[onvif_event.topic] =
                        onvif_event.time;
                }

                if (mapping.ext_proto_event_parse_cb) {
                    if (mapping.ext_proto_event_parse_cb(event, onvif_event))
                        notify(event);
                } else
                    notify(event);
            } else {
                logger->debug("No event mapping for topic {}",
                              onvif_event.topic);
            }
        }
    }

    virtual bool init() override {
        if (event_stream_) {
            logger->warn("Calling init() with allocated event stream!");
            return true;
        }

        event_stream_ = std::make_shared<rtsp_onvif_stream>(
            std::bind(&event_stream::on_onvif_event, this,
                      std::placeholders::_1),
            [this](vxg::media::Streamer::StreamError error) {
                if (error != vxg::media::Streamer::E_EOS && event_stream_) {
                    vxg::logger::warn("ONVIF stream error, restarting");
                    // Restart in a separate thread because we are not able
                    // to serialize stop/start from the same source streaming
                    // thread(we are here actually in the streaming thread now).
                    std::thread t([this]() {
                        event_stream_->stop_sink();
                        event_stream_->stop();
                        event_stream_->start(onvif_rtsp_url_);
                        event_stream_->start_sink();
                    });
                    t.detach();
                }
            });

        if (!event_stream_) {
            logger->error("Failed to allocate event stream");
            return false;
        }

        logger->info("Event stream allocated");
        return true;
    }

    virtual void finit() override {
        if (!event_stream_) {
            logger->warn("Calling finit() with deallocated event stream!");
            return;
        }

        // Unref event_stream_ object
        event_stream_ = nullptr;

        logger->info("Event stream deallocated");
    }

    virtual bool set_trigger_recording(bool enabled, int pre, int post) {
        logger->info("Event stream record trigger {} pre: {} post: {}",
                     enabled ? "enabled" : "disabled", pre, post);

        trigger_ = enabled;

        return true;
    }
};
using event_stream_ptr = std::shared_ptr<event_stream>;

}  // namespace onvif
}  // namespace media
}  // namespace vxg
#endif