#pragma once

#include <aws/core/Aws.h>
#include <aws/core/utils/StringUtils.h>
#include <aws/core/utils/memory/AWSMemory.h>
#include <aws/core/utils/memory/stl/AWSStreamFwd.h>
#include <aws/core/utils/stream/PreallocatedStreamBuf.h>
#include <aws/core/utils/threading/Executor.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/CreateBucketRequest.h>
#include <aws/s3/model/ListObjectsV2Request.h>
#include <aws/s3/model/Object.h>
#include <aws/s3/model/PutBucketLifecycleConfigurationRequest.h>
#include <aws/transfer/TransferHandle.h>
#include <aws/transfer/TransferManager.h>

#include <fstream>

#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/utils/logging/LogLevel.h>

#include "status-message.h"
#include "timeline.h"

namespace vxg {
namespace cloud {
namespace s3 {
class storage : public timed_storage, public status_messages {
    vxg::logger::logger_ptr logger {vxg::logger::instance("s3-storage")};

    // Init AWS SDK only once per process
    static std::once_flag aws_api_init_flag_;

public:
    struct options {
        std::string aws_access_id;
        std::string aws_secret_key;
        std::string s3_endpoint;
        std::string s3_bucket;
        ssize_t s3_bucket_recycle_after_days {-1};
        std::string s3_prefix_path;
        size_t max_concurent_uploads {2};
        bool insecure {false};
        bool validate_certs {true};
        std::chrono::milliseconds s3_connect_timeout {
            std::chrono::milliseconds(5000)};
        std::chrono::milliseconds s3_request_timeout {
            std::chrono::milliseconds(5000)};
    };

private:
    options options_;

    std::shared_ptr<Aws::S3::S3Client> s3_client_;
    std::shared_ptr<Aws::Utils::Threading::PooledThreadExecutor> s3_executor_;
    std::shared_ptr<Aws::Transfer::TransferManager> s3_transfer_manager_;

    void _init_aws_sdk();
    Aws::Vector<Aws::S3::Model::Object> _aws_s3_list_objects(
        const cloud::time& start,
        const cloud::time& end);
    Aws::Vector<Aws::S3::Model::Object> _aws_s3_list_objects_with_prefix(
        const Aws::String& prefix = "");
    void _aws_put_object_async(
        const std::string& object_name,
        const std::string& object_type,
        const std::vector<uint8_t>& data,
        const sync::timeline::async_store_finished_cb store_finished_cb);
    void _aws_put_object_async_finished(
        const Aws::S3::S3Client* s3Client,
        const Aws::S3::Model::PutObjectRequest& request,
        const Aws::S3::Model::PutObjectOutcome& outcome,
        const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context,
        const sync::timeline::async_store_finished_cb store_finished_cb);
    bool _aws_s3_storage_get_bucket(const std::string& bucket);
    bool _aws_s3_storage_create_bucket(const std::string& bucket);
    std::string _object_prefix_from_timepoint(const cloud::time& time);
    bool _period_from_filename(std::string filename, period& p);
    bool _filename_from_item(timed_storage::item_ptr, std::string& filename);
    bool _aws_use_bucket();
    bool _aws_s3_storage_set_bucket_lifecycle();

public:
    storage(options options, status_message_func status_cb = nullptr);
    virtual ~storage();
    bool init();
    void finit();

    virtual std::vector<item_ptr> list(cloud::time start,
                                       cloud::time stop) override;
    virtual bool load(item_ptr item) override;
    virtual bool store(item_ptr item) override;
    virtual void erase(item_ptr) override;

    bool store_async(timed_storage::item_ptr item,
                     sync::timeline::async_store_finished_cb finished_cb,
                     sync::timeline::async_store_is_canceled_cb is_canceled_cb);
};
}  // namespace s3
}  // namespace cloud
}  // namespace vxg