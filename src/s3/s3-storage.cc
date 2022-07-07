#include "s3-storage.h"

#include <date/date.h>
#include <functional>

using namespace Aws;
using namespace Aws::Utils;
using namespace Aws::S3;
using namespace vxg::cloud;

/**
 * In-memory stream implementation
 */
class MyUnderlyingStream : public Aws::IOStream {
public:
    using Base = Aws::IOStream;
    // Provide a customer-controlled streambuf to hold data from the bucket.
    MyUnderlyingStream(std::streambuf* buf) : Base(buf) {}

    virtual ~MyUnderlyingStream() = default;
};

namespace vxg {
namespace cloud {
namespace s3 {

struct async_context : public Aws::Client::AsyncCallerContext {
    using ptr = std::shared_ptr<async_context>;

    sync::timeline::async_store_finished_cb store_finished_cb;

    async_context(const std::string& object_name,
                  const sync::timeline::async_store_finished_cb& finished_cb)
        : Aws::Client::AsyncCallerContext("VXGAsyncS3Context"),
          store_finished_cb {finished_cb} {
        SetUUID(object_name);
    }
    virtual ~async_context() {}
};

bool storage::_period_from_filename(std::string filename, period& p) {
    using namespace vxg::cloud;
    namespace fs = std::experimental::filesystem;
    using namespace std::chrono;
    fs::path path(filename);
    filename = path.filename();

    auto timestamps = utils::string_split(filename, '_');

    p.begin = p.end = utils::time::null();

    if (timestamps.size() == 2) {
        p.begin = utils::time::from_iso_packed(timestamps[0]);
        p.end = utils::time::from_iso_packed(timestamps[1]);
    }

    if (!p.is_valid()) {
        logger->trace("Can't parse timestamps from '{}'", filename);
        return false;
    }

    return true;
}

std::string storage::_object_prefix_from_timepoint(const cloud::time& time) {
    namespace fs = std::experimental::filesystem;
    using namespace utils::time;
    using namespace std::chrono;
    fs::path path;

    // Main prefix; ex. {CamId}/
    path = fs::path(options_.s3_prefix_path);

    // Object's intermidiate prefix: /{YYYYMMDD}/{HH}/
    auto tp = time;
    auto dp = date::floor<date::days>(tp);
    date::year_month_day ymd {dp};
    auto t = date::make_time(
        std::chrono::duration_cast<std::chrono::milliseconds>(tp - dp));
    path /= utils::string_format(
        "%04i%02u%02u/%02u/", static_cast<int>(ymd.year()),
        static_cast<unsigned>(ymd.month()), static_cast<unsigned>(ymd.day()),
        t.hours().count());
    return path;
}

bool storage::_filename_from_item(timed_storage::item_ptr item,
                                  std::string& filename) {
    namespace fs = std::experimental::filesystem;
    using namespace utils::time;
    using namespace std::chrono;
    fs::path path;

    if (item->is_null() || item->is_open())
        return false;

    // Object's directory prefix
    path = _object_prefix_from_timepoint(item->begin);
    // Object's filename {ISO_Packed}_{ISO_Packed}.{Extention}
    path /=
        utils::string_format("%s_%s.mp4", to_iso_packed(item->begin).c_str(),
                             to_iso_packed(item->end).c_str());

    filename = path;

    return true;
}

Aws::Vector<Aws::S3::Model::Object> storage::_aws_s3_list_objects(
    const cloud::time& start,
    const cloud::time& end) {
    using namespace std::chrono;
    Aws::Vector<Aws::S3::Model::Object> result;

    // Directories structure we use provides per hour objects grouping.
    // Loop over directories in [start, stop] period with step of 1 hour.
    for (auto tp = start; tp <= end; tp += hours(1)) {
        auto objects =
            _aws_s3_list_objects_with_prefix(_object_prefix_from_timepoint(tp));
        // Append objects to result objects list
        result.insert(result.end(), objects.begin(), objects.end());
    }

    logger->debug("Objects listed in bucket '{}':", options_.s3_bucket);
    for (Aws::S3::Model::Object& object : result) {
        logger->debug("{}", object.GetKey());
    }

    return result;
}

Aws::Vector<Aws::S3::Model::Object> storage::_aws_s3_list_objects_with_prefix(
    const Aws::String& prefix) {
    Aws::Vector<Aws::S3::Model::Object> result;
    Aws::S3::Model::ListObjectsV2Request request;
    Aws::S3::Model::ListObjectsV2Outcome outcome;

    request.SetBucket(options_.s3_bucket);
    // 1000 objects max per request
    request.SetMaxKeys(1000);
    request.SetPrefix(prefix);

    do {
        outcome = s3_client_->ListObjectsV2(request);

        if (outcome.IsSuccess()) {
            Aws::Vector<Aws::S3::Model::Object> objects =
                outcome.GetResult().GetContents();

            if (!objects.empty()) {
                // Append objects to result objects list
                result.insert(result.end(), objects.begin(), objects.end());

                // Set next request start after the last object of previous
                // request
                request.SetStartAfter(objects.back().GetKey());
            }
        } else {
            std::string msg =
                "Error: ListObjects: " + outcome.GetError().GetMessage();
            logger->error(msg);
            report_status(msg);
            break;
        }
    } while (outcome.GetResult().GetIsTruncated());

    return result;
}

void storage::_aws_put_object_async_finished(
    const Aws::S3::S3Client* s3Client,
    const Aws::S3::Model::PutObjectRequest& request,
    const Aws::S3::Model::PutObjectOutcome& outcome,
    const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context,
    const sync::timeline::async_store_finished_cb store_finished_cb) {
    auto ok = outcome.IsSuccess();

    if (ok)
        logger->info("Success: PutObjectAsyncFinished: Finished uploading '{}'",
                     context->GetUUID());
    else
        logger->error("Error: PutObjectAsyncFinished: {}",
                      outcome.GetError().GetMessage());

    store_finished_cb(ok);
}

void storage::_aws_put_object_async(
    const std::string& object_name,
    const std::string& object_type,
    const std::vector<uint8_t>& data,
    const sync::timeline::async_store_finished_cb store_finished_cb) {
    // Create and configure the asynchronous put object request.
    Aws::S3::Model::PutObjectRequest request;
    request.SetBucket(options_.s3_bucket);
    request.SetKey(object_name);

    const std::shared_ptr<Aws::IOStream> input_data =
        Aws::MakeShared<Aws::StringStream>(
            "SampleAllocationTag", object_name.c_str(),
            std::stringstream::out | std::ios_base::in | std::ios_base::binary);
    // TODO: get rid of copy if possible
    input_data->write(reinterpret_cast<const char*>(data.data()), data.size());
    request.SetBody(input_data);
    request.SetContentType(object_type);

    logger->info("Uploading S3 object of {:.3f} Mb as {}.",
                 (double)data.size() / (1024 * 1024), object_name);

    // Create and configure the context for the asynchronous put object request.
    std::shared_ptr<async_context> context = Aws::MakeShared<async_context>(
        "AsyncPutContextTag", object_name, store_finished_cb);

    // Make the asynchronous put object call. Queue the request into a
    // thread executor and call the PutObjectAsyncFinished function when the
    // operation has finished.
    // s3_client_->PutObjectAsync(
    //     request,
    //     std::bind(&storage::_aws_put_object_async_finished, this,
    //               std::placeholders::_1, std::placeholders::_2,
    //               std::placeholders::_3, std::placeholders::_4,
    //               store_finished_cb),
    //     context);

    s3_transfer_manager_->UploadFile(
        input_data, options_.s3_bucket, object_name, object_type, {},
        std::dynamic_pointer_cast<Aws::Client::AsyncCallerContext>(context));
}

bool storage::_aws_s3_storage_get_bucket(const std::string& bucket_name) {
    Aws::S3::Model::ListBucketsOutcome outcome = s3_client_->ListBuckets();
    std::string msg;

    if (outcome.IsSuccess()) {
        logger->info("Looking for a bucket named '{}'", bucket_name);

        Aws::Vector<Aws::S3::Model::Bucket> bucket_list =
            outcome.GetResult().GetBuckets();

        for (Aws::S3::Model::Bucket const& bucket : bucket_list) {
            if (bucket.GetName() == bucket_name) {
                logger->info("Found the bucket.");

                return true;
            }
        }

        logger->info("Could not find bucket {}, trying to create it.",
                     bucket_name);
        if (_aws_s3_storage_create_bucket(bucket_name))
            return true;
        else
            msg = "Bucket " + bucket_name +
                  " not found and could not be created!";
    } else {
        msg = "ListBuckets error: " + outcome.GetError().GetMessage();
    }

    logger->error(msg);
    report_status(msg);

    return false;
}

bool storage::_aws_s3_storage_create_bucket(const std::string& bucket_name) {
    auto region = Aws::S3::Model::BucketLocationConstraint::us_east_1;

    Aws::S3::Model::CreateBucketRequest request;
    request.SetBucket(bucket_name);

    //  If you don't specify a AWS Region, the bucket is created in the US East
    //  (N. Virginia) Region (us-east-1)
    if (region != Aws::S3::Model::BucketLocationConstraint::us_east_1) {
        Aws::S3::Model::CreateBucketConfiguration bucket_config;
        bucket_config.SetLocationConstraint(region);

        request.SetCreateBucketConfiguration(bucket_config);
    }

    Aws::S3::Model::CreateBucketOutcome outcome =
        s3_client_->CreateBucket(request);

    if (!outcome.IsSuccess()) {
        auto err = outcome.GetError();
        logger->error("Error: CreateBucket: {}:{}", err.GetExceptionName(),
                      err.GetMessage());
        return false;
    }

    return true;
}

bool storage::_aws_s3_storage_set_bucket_lifecycle() {
    Aws::S3::Model::LifecycleRule lifecycle_rule;
    Aws::S3::Model::LifecycleRuleFilter lifecycle_rule_filter;
    Aws::S3::Model::LifecycleExpiration lifecycle_expiration;
    Aws::S3::Model::ExpirationStatus lifecycle_expiration_status {
        Aws::S3::Model::ExpirationStatus::Disabled};
    Aws::S3::Model::BucketLifecycleConfiguration lifecycle_config;
    Aws::S3::Model::PutBucketLifecycleConfigurationRequest request;

    // Prefix.
    lifecycle_rule_filter.SetPrefix(options_.s3_prefix_path);
    // 0 means disable recycling.
    if (options_.s3_bucket_recycle_after_days > 0) {
        logger->info(
            "Enabling objects recycling for prefix '{}' after {} days.",
            options_.s3_prefix_path, options_.s3_bucket_recycle_after_days);
        lifecycle_expiration_status = Aws::S3::Model::ExpirationStatus::Enabled;
        lifecycle_expiration.WithDays(options_.s3_bucket_recycle_after_days);
    } else {
        logger->info("Disabling objects recycling for prefix '{}'.",
                     options_.s3_prefix_path);
        lifecycle_expiration.WithDays(3);
        lifecycle_expiration_status =
            Aws::S3::Model::ExpirationStatus::Disabled;
    }

    lifecycle_rule.WithExpiration(lifecycle_expiration)
        .WithStatus(lifecycle_expiration_status)
        .WithFilter(lifecycle_rule_filter);
    lifecycle_config.WithRules({lifecycle_rule});

    request.WithBucket(options_.s3_bucket);
    request.WithLifecycleConfiguration(lifecycle_config);

    Aws::S3::Model::PutBucketLifecycleConfigurationOutcome outcome =
        s3_client_->PutBucketLifecycleConfiguration(request);

    if (!outcome.IsSuccess()) {
        auto err = outcome.GetError();
        logger->error("Error: PutBucketLifecycleConfiguration: {}:{}",
                      err.GetExceptionName(), err.GetMessage());
        return false;
    }

    logger->info(
        "Bucket '{}' recycling config successfully set for prefix '{}'",
        options_.s3_bucket, options_.s3_prefix_path);

    return true;
}

bool storage::_aws_use_bucket() {
    return _aws_s3_storage_get_bucket(options_.s3_bucket);
}

void storage::_init_aws_sdk() {
    try {
        std::call_once(storage::aws_api_init_flag_, []() {
            Aws::SDKOptions sdk_options_;
            sdk_options_.loggingOptions.logLevel =
                Aws::Utils::Logging::LogLevel::Debug;
            Aws::InitAPI(sdk_options_);
        });
    } catch (...) {
        std::string msg = "Failed to init AWS SDK";
        logger->error(msg);
        report_status(msg);
        throw new std::runtime_error(msg);
    }
}

storage::storage(options options, status_message_func cb)
    : options_ {options}, status_messages(cb) {
    _init_aws_sdk();
}

storage::~storage() {
    finit();
}

bool storage::init() {
    Aws::Client::ClientConfiguration config;
    config.endpointOverride = options_.s3_endpoint;
    config.scheme =
        options_.insecure ? Aws::Http::Scheme::HTTP : Aws::Http::Scheme::HTTPS;
    config.verifySSL = options_.validate_certs;
    config.connectTimeoutMs = options_.s3_connect_timeout.count();
    config.requestTimeoutMs = options_.s3_request_timeout.count();

    s3_client_ = Aws::MakeShared<Aws::S3::S3Client>(
        "S3Client",
        Aws::Auth::AWSCredentials(options_.aws_access_id,
                                  options_.aws_secret_key),
        config, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Always,
        false);

    if (!_aws_s3_storage_get_bucket(options_.s3_bucket)) {
        logger->error("S3 bucket {} access failed.", options_.s3_bucket);
        return false;
    }
    _aws_s3_storage_set_bucket_lifecycle();

    s3_executor_ = Aws::MakeShared<Aws::Utils::Threading::PooledThreadExecutor>(
        "executor", options_.max_concurent_uploads);
    Aws::Transfer::TransferManagerConfiguration transfer_config(
        s3_executor_.get());
    transfer_config.s3Client = s3_client_;
    // Ask AWS SDK to check data integrity
    transfer_config.computeContentMD5 = true;
    // Async transfer callback
    transfer_config.transferStatusUpdatedCallback =
        [&](const Aws::Transfer::TransferManager* tm,
            const std::shared_ptr<const Aws::Transfer::TransferHandle>& th) {
            auto context_orig =
                std::const_pointer_cast<Aws::Client::AsyncCallerContext>(
                    th->GetContext());
            auto context =
                std::dynamic_pointer_cast<async_context>(context_orig);
            auto handle =
                std::const_pointer_cast<Aws::Transfer::TransferHandle>(th);

            Aws::Transfer::PartStateMap queued, pending, failed, completed;
            handle->GetAllPartsTransactional(queued, pending, failed,
                                             completed);

            // Notify we are done with all parts
            if (queued.size() == 0 && pending.size() == 0) {
                bool ok =
                    (th->GetStatus() == Transfer::TransferStatus::COMPLETED);
                logger->info("S3 storage upload {}.",
                             ok ? "completed" : "failed");
                if (context->store_finished_cb)
                    context->store_finished_cb(ok);
            }
        };

    s3_transfer_manager_ =
        Aws::Transfer::TransferManager::Create(transfer_config);
    if (s3_transfer_manager_ != nullptr) {
        report_status("OK");
        return true;
    }
    return false;
}

void storage::finit() {}

std::vector<timed_storage::item_ptr> storage::list(cloud::time start,
                                                   cloud::time stop) {
    namespace fs = std::experimental::filesystem;
    using namespace vxg::cloud::utils::time;
    std::vector<item_ptr> result;

    auto aws_objects = _aws_s3_list_objects(start, stop);

    for (auto& o : aws_objects) {
        vxg::cloud::period p;
        if (_period_from_filename(o.GetKey(), p) &&
            p.intersects({start, stop})) {
            result.push_back(std::make_shared<item>(p));
        }
    }

    return result;
}

bool storage::load(item_ptr item) {
    using namespace vxg::cloud::utils::time;

    // TODO:

    return false;
}

bool storage::store(item_ptr item) {
    namespace lws = vxg::cloud::transport::libwebsockets;
    using namespace vxg::cloud::utils::time;
    using namespace std::chrono;

    // TODO:

    return false;
}

bool storage::store_async(
    timed_storage::item_ptr item,
    sync::timeline::async_store_finished_cb finished_cb,
    sync::timeline::async_store_is_canceled_cb is_canceled_cb) {
    std::string object_name;

    if (_filename_from_item(item, object_name)) {
        _aws_put_object_async(object_name, "video/mp4", item->data,
                              finished_cb);
        return true;
    }

    return false;
}

void storage::erase(item_ptr) {}

std::once_flag storage::aws_api_init_flag_;
}  // namespace s3
}  // namespace cloud
}  // namespace vxg