#ifndef __COMMAND_AUDIO_FILE_H
#define __COMMAND_AUDIO_FILE_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

//! 4.16 audio_file_play (SRV)
//! Camera should download and play an external audio file.
struct audio_file_play : public base_command {
    audio_file_play() { cmd = AUDIO_FILE_PLAY; }

    virtual ~audio_file_play() {}

    // url: string, URL of a file that should be played
    std::string url;

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(audio_file_play, base_command, url);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
