#include <gtest/gtest.h>

#include <streamer/rtsp_onvif_metadata_event_stream.h>

using namespace ::testing;
using namespace std;
using namespace vxg::media;

constexpr char* RTSP_ONVIF_METADATA_URL =
    "rtsp://root:secret@ip:554/axis-media/"
    "media.amp?event=on";

class TestMetaEvenetStream : public Test {
    void cb(onvif::event e) { vxg::logger::info("{}", e.raw_body); }

protected:
    TestMetaEvenetStream() {
        onvif::onvif_topic_to_vxg_event_map mapping;
        vxg::cloud::agent::proto::event_config config;
        config.event = vxg::cloud::agent::proto::ET_CUSTOM;
        config.custom_event_name = "onvif-event";

        mapping.onvif_topic = "*";
        mapping.vxg_event_config = config;

        std::map<std::string, onvif::onvif_topic_to_vxg_event_map> event_map;
        event_map["*"] = mapping;

        stream_ = std::make_shared<onvif::event_stream>(RTSP_ONVIF_METADATA_URL,
                                                        event_map);
    }

    virtual void SetUp() { stream_->start(); }

    virtual void TearDown() { stream_->stop(); }

public:
    onvif::event_stream_ptr stream_;
};

TEST_F(TestMetaEvenetStream, Sink) {
    sleep(100);
}
