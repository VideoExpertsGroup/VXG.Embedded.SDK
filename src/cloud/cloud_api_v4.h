#pragma once

#include <agent-proto/proto.h>
#include <string>

#include <cloud/cloud_api_v2.h>

namespace vxg {
namespace cloud {
namespace api {

namespace v4 {
const std::string ENDPOINT = "/api/v4/";

namespace storage {
const std::string ENDPOINT = v4::ENDPOINT + "storage/";

typedef v2::storage::meta_object meta_object;

namespace records {
const std::string ENDPOINT = v4::storage::ENDPOINT + "records/";

typedef v2::storage::data::object_object object_object;
typedef v2::storage::data::response response;

namespace upload {
const std::string ENDPOINT = v4::storage::records::ENDPOINT + "upload/";

struct request {
    int duration {UnsetInt};
    int size {UnsetInt};
    cloud::time time {UnsetTime};

    JSON_DEFINE_TYPE_INTRUSIVE(request, duration, size, time);
};

struct response {
    cloud::time expire {UnsetTime};
    std::vector<std::string> urls;

    JSON_DEFINE_TYPE_INTRUSIVE(response, expire, urls);
};

}  // namespace upload
}  // namespace records
}  // namespace storage
}  // namespace v4
}  // namespace api
}  // namespace cloud
}  // namespace vxg