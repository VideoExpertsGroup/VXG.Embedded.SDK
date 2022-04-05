#pragma once

#include <agent-proto/proto.h>
#include <string>

namespace vxg {
namespace cloud {
namespace api {
namespace v2 {

const std::string ENDPOINT = "/api/v2/";

namespace storage {
const std::string ENDPOINT = api::v2::ENDPOINT + "storage/";

struct meta_object {
    cloud::time expire;
    int limit {UnsetInt};
    std::string next {UnsetString};
    int offset {UnsetInt};
    std::string previous {UnsetString};
    int total_count {UnsetInt};

    JSON_DEFINE_TYPE_INTRUSIVE(meta_object,
                               expire,
                               limit,
                               next,
                               offset,
                               previous,
                               total_count);
};

namespace data {
const std::string ENDPOINT = storage::ENDPOINT + "data/?";

struct object_object {
    int camid {UnsetInt};
    cloud::time start;
    cloud::time end;
    int id {UnsetInt};
    int size {UnsetInt};
    std::string url {UnsetString};

    JSON_DEFINE_TYPE_INTRUSIVE(object_object, camid, start, end, id, size, url);
};

struct response {
    meta_object meta;
    std::vector<object_object> objects;

    JSON_DEFINE_TYPE_INTRUSIVE(response, meta, objects);
};

}  // namespace data
}  // namespace storage
}  // namespace v2
}  // namespace api
}  // namespace cloud
}  // namespace vxg