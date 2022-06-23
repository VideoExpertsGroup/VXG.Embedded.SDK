#pragma once

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 5.6 audio_detection_conf (CM)
//! Current audio detection settings. Reply 5.4 get_audio_detection (SRV).
struct audio_detection_conf : public base_command,
                              public proto::audio_detection_config {
    audio_detection_conf() { cmd = AUDIO_DETECTION_CONF; }

    virtual ~audio_detection_conf() {}

    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(audio_detection_conf,
                                                base_command,
                                                proto::audio_detection_config);
};

//! 5.4 get_audio_detection (SRV)
//! Get audio detection parameters.
struct get_audio_detection : public base_command {
    get_audio_detection() { cmd = GET_AUDIO_DETECTION; }

    virtual ~get_audio_detection() {}

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE_NO_ARGS(get_audio_detection,
                                               base_command);
};

//! 5.5 set_audio_detection (SRV)
//! Apply audio detection parameters.
struct set_audio_detection : public base_command,
                             public proto::audio_detection_config {
    set_audio_detection() { cmd = SET_AUDIO_DETECTION; }

    virtual ~set_audio_detection() {}

    JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(set_audio_detection,
                                                base_command,
                                                audio_detection_config);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
