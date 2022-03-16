#ifndef __ONVIF_METADATA_SINK_H
#define __ONVIF_METADATA_SINK_H

#include <streamer/base_streamer.h>
#include <nlohmann/json.hpp>
#include <xml2json.hpp>

namespace vxg {
namespace media {
namespace onvif {

class data_sink : public vxg::media::Streamer::ISink {
    vxg::logger::logger_ptr logger = vxg::logger::instance(name());

public:
    data_sink(
        std::function<void(std::vector<uint8_t>)> cb = nullptr,
        std::function<void(vxg::media::Streamer::StreamError)> err_cb = nullptr)
        : cb_ {cb}, error_cb_ {err_cb} {}

    virtual ~data_sink() {}

    virtual bool init(std::string) override { return true; }

    virtual bool finit() override { return true; }

    virtual bool droppable() override { return false; }

    virtual std::string name() override { return "data-sink"; }

    virtual void error(vxg::media::Streamer::StreamError error) override {
        if (error_cb_)
            error_cb_(error);
    }

    virtual bool process(
        std::shared_ptr<vxg::media::Streamer::MediaFrame> data) {
        if (data->type == vxg::media::Streamer::MediaType::DATA) {
            logger->trace("Data of size {} received", data->len);

            if (cb_)
                cb_(data->data);
        }

        return true;
    }

private:
    std::function<void(std::vector<uint8_t>)> cb_;
    std::function<void(vxg::media::Streamer::StreamError)> error_cb_;
};

struct simple_item {
    std::string name;
    std::string value;
};

struct event {
    std::string raw_body;

    std::string topic;
    cloud::time time;
    std::string prop_op;

    std::vector<simple_item> source_items;
    std::vector<simple_item> data_items;
};

class metadata_sink : public data_sink {
    vxg::logger::logger_ptr logger = vxg::logger::instance(name());
    std::string aggregated_data_;
    size_t num_aggregated_packets {0};
    const size_t max_aggregated_packets_ {3};

public:
    metadata_sink(
        std::function<void(event)> event_cb = nullptr,
        std::function<void(vxg::media::Streamer::StreamError)> err_cb = nullptr)
        : cb_ {event_cb}, data_sink(nullptr, err_cb) {}

    virtual ~metadata_sink() {}

    virtual std::string name() override { return "onvif-metadata-sink"; }

    virtual bool init(std::string) override {
        _clear_aggregated();
        return true;
    }

private:
    bool _parse_simple_items(rapidxml::xml_node<>* instance,
                             std::vector<simple_item>& items) {
        if (instance) {
            for (auto item = instance->first_node(); item != nullptr;
                 item = item->next_sibling()) {
                auto name_attr = item->first_attribute("Name");
                auto value_attr = item->first_attribute("Value");
                simple_item _item;

                if (value_attr && value_attr->value())
                    _item.value = value_attr->value();
                if (name_attr && name_attr->value())
                    _item.name = name_attr->value();

                items.push_back(_item);
            }

            return true;
        }

        return false;
    }

    event _parse_aggregated_event() {
        using namespace vxg::cloud;

        event event;
        event.raw_body = aggregated_data_;

        // No xpath in rapidxml so we have to parse by hands
        // MetadataStream
        //      Event
        //          NotificationMessage
        //              Topic
        //              ProducerReference
        //                  Address
        //              Message
        //                  Message[UtcTime,PropertyOperation]
        //                      Source
        //                          SimpleItem
        //                      Key?
        //                      Data
        //                          SimpleItem
        rapidxml::xml_document<>* xml_doc = new rapidxml::xml_document<>();
        std::string tmp_body = aggregated_data_;
        xml_doc->parse<0>(const_cast<char*>(tmp_body.data()));

        // MetadataStream
        rapidxml::xml_node<>* node = xml_doc->first_node(
            "MetadataStream", "http://www.onvif.org/ver10/schema");

        // Event
        if (node && (node = node->first_node("Event"))) {
            // NotificationMessage
            if (node && (node = node->first_node(
                             "NotificationMessage",
                             "http://docs.oasis-open.org/wsn/b-2"))) {
                // Topic
                rapidxml::xml_node<>* topic_node = node->first_node("Topic");
                event.topic = topic_node->value();

                //      ProducerReference/Address
                rapidxml::xml_node<>* producer_node =
                    node->first_node("ProducerReference");
                if (producer_node)
                    producer_node = producer_node->first_node("Address");

                //          Message/Message
                rapidxml::xml_node<>* message_node =
                    node->first_node("Message");
                if (message_node &&
                    (message_node = message_node->first_node(
                         "Message", "http://www.onvif.org/ver10/schema"))) {
                    //          Message:UtcTime
                    rapidxml::xml_attribute<>* utc_time_attr =
                        message_node->first_attribute("UtcTime");
                    if (utc_time_attr)
                        event.time =
                            utils::time::from_iso(utc_time_attr->value());

                    //          Message:PropertyOperation
                    rapidxml::xml_attribute<>* prop_op_attr =
                        message_node->first_attribute("PropertyOperation");
                    if (prop_op_attr)
                        event.prop_op = prop_op_attr->value();

                    //              Source
                    rapidxml::xml_node<>* source_node =
                        message_node->first_node("Source");
                    //              Source/[SimpleItem]
                    _parse_simple_items(source_node, event.source_items);

                    //              Data
                    rapidxml::xml_node<>* data_node =
                        message_node->first_node("Data");
                    //              Data/[SimpleItem]
                    _parse_simple_items(data_node, event.data_items);
                }
            }
        }

        delete xml_doc;

        return event;
    }

    void _add_aggregated(std::string data) {
        aggregated_data_ += data;
        num_aggregated_packets++;
    }

    void _clear_aggregated() {
        num_aggregated_packets = 0;
        aggregated_data_.clear();
    }

    bool _validate_aggregated() {
        rapidxml::xml_document<>* xml_doc = new rapidxml::xml_document<>();
        bool valid_xml = false;

        try {
            xml_doc->parse<rapidxml::parse_non_destructive>(
                const_cast<char*>(aggregated_data_.data()));
            valid_xml = true;
        } catch (...) {
            if (num_aggregated_packets > max_aggregated_packets_)
                _clear_aggregated();
        }

        delete xml_doc;

        return valid_xml;
    }

    bool _aggregate_check_data(std::string new_data) {
        _add_aggregated(new_data);
        return _validate_aggregated();
    }

    virtual bool process(
        std::shared_ptr<vxg::media::Streamer::MediaFrame> metadata) override {
        if (metadata->type == vxg::media::Streamer::MediaType::DATA) {
            std::string data(metadata->data.begin(), metadata->data.end());

            if (_aggregate_check_data(data)) {
                if (cb_) {
                    auto event = _parse_aggregated_event();
                    _clear_aggregated();
                    cb_(event);
                }
            }
        }

        return true;
    }

private:
    std::function<void(event)> cb_;
    std::function<void()> error_cb_;
};

}  // namespace onvif
}  // namespace media
}  // namespace vxg
#endif