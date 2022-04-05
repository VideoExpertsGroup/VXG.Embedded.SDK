#ifndef __COMMAND_GET_STREAM_CONFIG_H
#define __COMMAND_GET_STREAM_CONFIG_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 4.6 stream_config (CM)
//! Send current video and audio parameters from camera. Reply 4.4
//! get_stream_config (SRV).
struct stream_config : public base_command, public proto::stream_config {
    typedef std::shared_ptr<stream_config> ptr;

    stream_config() { cmd = STREAM_CONFIG; }

    virtual ~stream_config() {}

    //! XXX
    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(stream_config,
                                                base_command,
                                                proto::stream_config);
};
//! 4.4 get_stream_config (SRV)
//! Get video and audio parameters from camera.
struct get_stream_config : public base_command {
    typedef std::shared_ptr<get_stream_config> ptr;

    get_stream_config() { cmd = GET_STREAM_CONFIG; }

    virtual ~get_stream_config() {}

    //! video_es: optional list of string, list of video ES for which
    //! configuration should be returned; if parameter is not specified, it
    //! means all supported streams request; if the list is empty, no streams
    //! are requested
    std::vector<std::string> video_es {UnsetString};
    //! audio_es: optional list of string, list of audio ES for which
    //! configuration should be returned; rules are the same as for video
    std::vector<std::string> audio_es {UnsetString};

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(get_stream_config,
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
