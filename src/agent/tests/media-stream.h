#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <agent/stream.h>

using namespace vxg::media;
using namespace vxg::cloud;
using namespace vxg::cloud::agent;
using namespace ::testing;

struct mock_media_source : public Streamer::ISource {
    mock_media_source() {}
    virtual ~mock_media_source() { ; }

    MOCK_METHOD(bool, init, (std::string), (override));
    MOCK_METHOD(void, finit, (), (override));
    std::vector<Streamer::StreamInfo> negotiate() override { return {}; }
    MOCK_METHOD(std::shared_ptr<Streamer::MediaFrame>,
                pullFrame,
                (),
                (override));
    MOCK_METHOD(std::string, name, (), (override));
};
using mock_media_source_ptr = std::shared_ptr<mock_media_source>;

struct mock_media_sink : public Streamer::ISink {
    mock_media_sink() {}
    virtual ~mock_media_sink() { ; }

    MOCK_METHOD(bool, init, (std::string), (override));
    MOCK_METHOD(bool, finit, (), (override));
    MOCK_METHOD(bool,
                negotiate,
                (std::vector<Streamer::StreamInfo>),
                (override));
    MOCK_METHOD(bool,
                process,
                (std::shared_ptr<Streamer::MediaFrame>),
                (override));
    MOCK_METHOD(std::string, name, (), (override));
    virtual bool droppable() override { return true; }
    virtual void error(Streamer::StreamError error) override {}
};
using mock_media_sink_ptr = std::shared_ptr<mock_media_sink>;

constexpr char* DEFAULT_TEST_MEDIA_STREAM = "test-media-stream";
constexpr char* DEFAULT_TEST_VIDEO_ES = "test-video-es-0";
constexpr char* DEFAULT_TEST_AUDIO_ES = "test-audio-es-0";
struct mock_media_stream : public agent::media::stream {
public:
    mock_media_stream(
        const std::string& name = DEFAULT_TEST_MEDIA_STREAM,
        mock_media_source_ptr source = std::make_shared<mock_media_source>(),
        mock_media_sink_ptr sink = std::make_shared<mock_media_sink>())
        : agent::media::stream(name, source, false) {
        vxg::media::stream::sink_ = sink;

        ON_CALL(*this, get_supported_stream(_))
            .WillByDefault(Invoke(
                std::bind(&mock_media_stream::default_get_supported_stream,
                          this, std::placeholders::_1)));
        ON_CALL(*this, get_stream_caps(_))
            .WillByDefault(
                Invoke(std::bind(&mock_media_stream::default_get_stream_caps,
                                 this, std::placeholders::_1)));
    }
    virtual ~mock_media_stream() { ; }

    bool default_get_supported_stream(
        proto::supported_stream_config& supported_stream) {
        supported_stream.id = cloud_name();
        supported_stream.video = DEFAULT_TEST_VIDEO_ES;
        supported_stream.audio = DEFAULT_TEST_AUDIO_ES;
        return true;
    }

    bool default_get_stream_caps(proto::stream_caps& caps) {
        bool result = false;
        for (auto& vc : caps.caps_video) {
            if (vc.streams.back() == DEFAULT_TEST_VIDEO_ES) {
                vc.brt.push_back(256);
                vc.brt.push_back(2000);
                vc.brt.push_back(1);

                vc.formats.push_back(proto::VF_H264);

                vc.fps.push_back(1.0f);
                vc.fps.push_back(15.0f);
                vc.fps.push_back(30.0f);

                vc.gop.push_back(1);
                vc.gop.push_back(30);
                vc.gop.push_back(1);

                vc.profiles.push_back(std::make_pair(proto::VF_H264, "main"));

                vc.quality = std::make_pair(0, 100);

                vc.resolutions.push_back(std::make_pair(1920, 1080));
                vc.resolutions.push_back(std::make_pair(320, 240));

                vc.smoothing = false;
                vc.vbr = true;
                vc.vbr_brt = vc.brt;

                result = true;
            }
        }

        for (auto& ac : caps.caps_audio) {
            if (ac.streams.back() == DEFAULT_TEST_VIDEO_ES) {
                ac.formats.push_back(proto::AF_AAC);
                ac.formats.push_back(proto::AF_G711U);

                ac.brt.push_back(64);
                ac.brt.push_back(320);
                ac.brt.push_back(64);

                ac.srt.push_back(8000.0f);
                ac.srt.push_back(16000.0f);
                ac.srt.push_back(32000.0f);

                result = true;
            }
        }

        return result;
    }

    MOCK_METHOD(bool,
                get_supported_stream,
                (proto::supported_stream_config&),
                (override));
    MOCK_METHOD(bool, get_stream_caps, (proto::stream_caps&), (override));
    MOCK_METHOD(bool,
                get_stream_config,
                (vxg::cloud::agent::proto::stream_config&),
                (override));
    MOCK_METHOD(bool,
                set_stream_config,
                (const vxg::cloud::agent::proto::stream_config&),
                (override));
    MOCK_METHOD(bool,
                get_snapshot,
                (vxg::cloud::agent::proto::event_object::snapshot_info_object&),
                (override));
    MOCK_METHOD(bool, start_record, (), (override));
    MOCK_METHOD(bool, stop_record, (), (override));
    MOCK_METHOD(std::vector<vxg::cloud::agent::proto::video_clip_info>,
                record_get_list,
                (vxg::cloud::time, vxg::cloud::time, bool),
                (override));
    MOCK_METHOD(vxg::cloud::agent::proto::video_clip_info,
                record_export,
                (vxg::cloud::time, vxg::cloud::time),
                (override));
};
using mock_media_stream_ptr = std::shared_ptr<mock_media_stream>;
