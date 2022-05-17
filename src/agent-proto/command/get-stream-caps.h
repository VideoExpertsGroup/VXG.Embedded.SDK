#ifndef __COMMAND_GET_STREAM_CAPS_H
#define __COMMAND_GET_STREAM_CAPS_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 4.11 stream_caps (CM)
//! Report encoder capabilities. Reply 4.10 get_stream_caps (SRV).
struct stream_caps : public base_command, public proto::stream_caps {
    typedef std::shared_ptr<stream_caps> ptr;

    stream_caps() { cmd = STREAM_CAPS; }

    virtual ~stream_caps() {}

    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(stream_caps,
                                                base_command,
                                                proto::stream_caps);
};

//! 4.10 get_stream_caps (SRV)
//! Get video and audio encoding capabilities.
struct get_stream_caps : public base_command {
    typedef std::shared_ptr<get_stream_caps> ptr;

    get_stream_caps() { cmd = GET_STREAM_CAPS; }
    virtual ~get_stream_caps() {}

    //! optional list of string, list of video ES for which capabilities should
    //! be returned; if parameter is not specified, all supported streams should
    //! be returned; if the list is empty, no stream capabilities are returned
    std::vector<std::string> video_es {UnsetString};
    //! optional list of string, list of audio ES for which capabilities should
    //! be returned; rules are the same as for video
    std::vector<std::string> audio_es {UnsetString};

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(get_stream_caps,
                                       base_command,
                                       video_es,
                                       audio_es);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
