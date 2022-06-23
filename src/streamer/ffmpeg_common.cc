#include "ffmpeg_common.h"

#include <mutex>

namespace vxg {
namespace media {
namespace ffmpeg {
// static members initialization
std::once_flag common::ff_static_init_flag_;
}  // namespace ffmpeg
}  // namespace media
}  // namespace vxg
