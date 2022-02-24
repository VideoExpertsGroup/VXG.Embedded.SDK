#include "properties.h"

namespace vxg {
std::string properties::props_filename_;
std::string properties::default_props_filename_;
std::mutex properties::lock_;
nlohmann::json properties::default_json_;
}  // namespace vxg
