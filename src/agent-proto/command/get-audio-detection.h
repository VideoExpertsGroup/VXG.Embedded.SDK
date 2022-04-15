#ifndef __COMMAND_GET_AUDIO_DETECTION_H
#define __COMMAND_GET_AUDIO_DETECTION_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

struct audio_detection_conf_caps {
    //! level: [min:int, max:int, step:int], volume range and step in -dB
    std::vector<int> level;

    JSON_DEFINE_TYPE_INTRUSIVE(audio_detection_conf_caps, level);
};

bool operator==(const audio_detection_conf_caps,
                const audio_detection_conf_caps);

//! 5.6 audio_detection_config (CM)
//! Current audio detection settings. Reply 5.4 get_audio_detection (SRV).
struct audio_detection_config {
    //! level: int, audio volume in -dB
    int level {UnsetInt};
    //! length: int, duration before event trigger, msec
    int length {UnsetInt};
    //! caps:
    audio_detection_conf_caps caps;

    JSON_DEFINE_TYPE_INTRUSIVE(audio_detection_config, level, length, caps);
};

//! 5.6 audio_detection_conf (CM)
//! Current audio detection settings. Reply 5.4 get_audio_detection (SRV).
struct audio_detection_conf : public base_command,
                              public audio_detection_config {
    audio_detection_conf() { cmd = AUDIO_DETECTION_CONF; }

    virtual ~audio_detection_conf() {}

    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(audio_detection_conf,
                                                base_command,
                                                audio_detection_config);
};

//! 5.4 get_audio_detection (SRV)
//! Get audio detection parameters.
struct get_audio_detection : public base_command {
    get_audio_detection() { cmd = GET_AUDIO_DETECTION; }

    virtual ~get_audio_detection() {}

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE_NO_ARGS(get_audio_detection,
                                               base_command);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
