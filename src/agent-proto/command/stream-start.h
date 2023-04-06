#ifndef __COMMAND_STREAM_START_H
#define __COMMAND_STREAM_START_H

#include <agent-proto/command/command-list.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
namespace command {

enum stream_reason {
    SR_RECORD,
    SR_RECORD_BY_EVENT,
    SR_LIVE,
    SR_INVALID = -1,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( stream_reason, {
    // “record” - purpose of streaming is continuous recording;
    {SR_RECORD, "record"},
    // “record_by_event” - purpose of streaming is event-driven recording.
    //  In this case CM starts streams, uploads pre-event files according to
    //  rules of event-driven recording. “record” and “record_by_event” reasons
    //  are mutually exclusive reasons. i.e. the same stream can't be started
    //  for “record” and “record_by_event” reasons simultaneously.
    {SR_RECORD_BY_EVENT, "record_by_event"},
    // “live” - the purpose of streaming is monitoring
    {SR_LIVE, "live"},

    {SR_INVALID, nullptr},
})
// clang-format on

//! 4.2 stream_start (SRV)
//! CM must start streaming of working stream.
struct stream_start : public base_command {
    stream_start() { cmd = STREAM_START; }

    virtual ~stream_start() {}

    //! reason of stream staring.
    proto::stream_reason reason {proto::stream_reason::SR_INVALID};
    //! string, camera media stream to use
    std::string stream_id {UnsetString};
    //! optional int, server-defined ID of new streaming
    //! session. When exists, should be used in building
    //! streaming URL.
    int publish_session_id {UnsetInt};
    //! optional bool, When it’s ‘True’ camera is recording stream direct to S3
    //! Storage.
    alter_bool storage_direct_recording {alter_bool::B_INVALID};

    JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(stream_start,
                                       base_command,
                                       reason,
                                       stream_id,
                                       publish_session_id,
                                       storage_direct_recording);
};

}  // namespace command
}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
