#ifndef __COMMAND_CAM_HELLO_H
#define __COMMAND_CAM_HELLO_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

struct cam_hello : public base_command {
    cam_hello() { cmd = CAM_HELLO; }

    virtual ~cam_hello() {}

    // path: string, URL path used to build media stream URL; see 7 Data channel
    std::string path {UnsetString};
    // media_uri: optional string, streaming protocol, server address and port
    // for media streaming; if protocols is not specified, RTMP is expected
    std::string media_uri {UnsetString};
    // backward_uri: optional string, streaming protocol, server address and
    // port for backward audio streaming; if protocols is not specified, RTMP is
    // expected
    std::string backward_uri {UnsetString};
    // mode: optional string, “p2p” for P2P mode, “cloud” for full cloud mode
    std::string mode {UnsetString};
    // activity: bool, True – all camera's functions are active. Camera is in
    // interactive mode. False – camera is in a passive mode. In this mode
    // camera:
    // - Stays on line awaiting for activity mode changing or closing of the
    // connection;
    // - Receives, replies, keeps parameters of miscellaneous commands that can
    // conflict with below items, but doesn't process them. They must be applied
    // after activity mode changing;
    // - Suspends sending all kind of events;
    // - Suspends all kind of streaming excluding remaining post-event duration
    // for already reported/processed events;
    // - Completes uploading of unfinished pre-event duration;
    // - Switches off all available indicators, lighting (IR) and other
    // functionality (TDN etc).
    bool activity {false};

    // NIY
    // tunnel: optional struct, appears when mode is “p2p”, describes tunnel
    // connection settings: login: string, login to use; pass: string, password
    // to use; addr: string, tunneling server address; port: string, tunneling
    // server port.

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(cam_hello,
                                       base_command,
                                       path,
                                       media_uri,
                                       backward_uri,
                                       mode);
};
}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg

#endif
