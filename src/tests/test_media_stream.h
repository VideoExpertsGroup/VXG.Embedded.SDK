#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <agent/rtsp-stream.h>

struct mock_media_stream : public vxg::cloud::agent::media::rtsp_stream {
    mock_media_stream(std::string name)
        : vxg::cloud::agent::media::rtsp_stream("", name) {}
    MOCK_METHOD(std::vector<vxg::cloud::agent::proto::video_clip_info>,
                record_get_list,
                (vxg::cloud::time begin, vxg::cloud::time end, bool align),
                (override));

    MOCK_METHOD(vxg::cloud::agent::proto::video_clip_info,
                record_export,
                (vxg::cloud::time begin, vxg::cloud::time end),
                (override));
};