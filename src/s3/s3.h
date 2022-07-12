#pragma once

#include <agent-proto/objects/config.h>
#include <agent/stream.h>
#include <s3/event-manager.h>
#include <s3/s3-storage.h>
#include <s3/status-message.h>
#include <s3/stream-storage.h>
#include <s3/timeline-synchronizer.h>

namespace vxg {
namespace cloud {
namespace s3 {
class uploader : public status_messages {
    static constexpr char* TAG = "s3-uploader";
    vxg::logger::logger_ptr logger = vxg::logger::instance(TAG);

    sync::timeline_ptr local_timeline;
    sync::timeline_ptr remote_timeline;
    std::shared_ptr<vxg::cloud::synchronizer> segmented_uploader_;
    event_manager_ptr event_manager_;
    agent::proto::events_config event_configs_;

    struct s3_sync_cb : public event_manager::event_state_report_cb {
        std::shared_ptr<vxg::cloud::synchronizer> syncer_;
        std::chrono::milliseconds pre_ {
            agent::profile::global::instance().default_pre_record_time};
        std::chrono::milliseconds post_ {
            agent::profile::global::instance().default_post_record_time};

        s3_sync_cb(
            std::shared_ptr<vxg::cloud::synchronizer> syncer,
            std::chrono::milliseconds pre = std::chrono::milliseconds(5000),
            std::chrono::milliseconds post = std::chrono::milliseconds(5000))
            : syncer_ {syncer}, pre_ {pre}, post_ {post} {}
        virtual ~s3_sync_cb() {}

        virtual synchronizer::sync_request_ptr on_need_stream_sync_start(
            const cloud::time& start) override {
            return syncer_->sync(
                start - pre_, utils::time::null(),
                [&](int progress, synchronizer::sync_request_status status) {
                    if (status == synchronizer::sync_request_status::ERROR ||
                        status == synchronizer::sync_request_status::DONE)
                        vxg::logger::info("sync request done with status {}",
                                          status);
                });
        }

        virtual void on_need_stream_sync_stop(
            synchronizer::sync_request_ptr sync_request,
            const cloud::time& stop) {
            syncer_->sync_finalize(sync_request, stop + post_);
        }

        virtual void on_need_stream_sync_continue(
            synchronizer::sync_request_ptr sync_request,
            const cloud::time& stop) {
            vxg::logger::info("Sync continue");
        }
    };

public:
    enum class upload_mode {
        MODE_UNKNOWN,
        MODE_CONTINUOUS,
        MODE_EVENT,
    };

    struct options {
        storage::options s3_options;
        upload_mode initial_upload_mode {upload_mode::MODE_UNKNOWN};
        std::chrono::milliseconds recording_event_pre {
            agent::profile::global::instance().default_pre_record_time};
        std::chrono::milliseconds recording_event_post {
            agent::profile::global::instance().default_post_record_time};
    };

private:
    options options_;
    upload_mode mode_ {upload_mode::MODE_UNKNOWN};

    uploader(agent::media::stream::ptr stream,
             std::vector<agent::event_stream::ptr>&& event_streams,
             const options& options,
             status_message_func status_cb = nullptr)
        : status_messages(status_cb), options_ {options} {
        local_timeline = std::make_shared<sync::timeline>(
            std::move(std::make_shared<stream_storage>(stream)));
        remote_timeline = std::make_shared<sync::timeline>(
            std::move(std::make_shared<storage>(options.s3_options, get_cb())));

        segmented_uploader_ =
            vxg::cloud::synchronizer::create(local_timeline, remote_timeline);

        event_manager_ = std::make_shared<event_manager>(
            std::move(event_streams),
            std::make_shared<s3_sync_cb>(segmented_uploader_));
        event_manager_->start();
    }

    void _notify_continuos(bool active) {
        agent::proto::event_object e;
        e.event = agent::proto::ET_CUSTOM;
        e.custom_event_name = "timeline-sync";
        e.time = utils::time::to_double(utils::time::now());
        e.active = active;
        event_manager_->notify_event(e);
    }

public:
    using ptr = std::shared_ptr<uploader>;
    static ptr create(agent::media::stream::ptr stream,
                      std::vector<agent::event_stream::ptr>&& event_streams,
                      const options& options,
                      status_message_func status_cb = nullptr) {
        struct make_shared_enabler : public uploader {
            make_shared_enabler(
                agent::media::stream::ptr stream,
                std::vector<agent::event_stream::ptr>&& event_streams,
                const options& options,
                status_message_func status_cb = nullptr)
                : uploader(stream,
                           std::move(event_streams),
                           options,
                           status_cb) {}
        };
        auto result = std::make_shared<make_shared_enabler>(
            stream, std::move(event_streams), options, status_cb);
        if (!result || !result->segmented_uploader_->start()) {
            vxg::logger::instance(TAG)->error(
                "Unable to start timeline synchronizer!");
            return nullptr;
        }

        vxg::logger::instance(TAG)->info(
            "Creating timeline synchronizer with initial upload mode {}",
            options.initial_upload_mode);
        if (result) {
            // Start actual continuous or event driven upload
            result->switch_mode(options.initial_upload_mode);
        }
        return result;
    }
    virtual ~uploader() {}

    void synchronize(cloud::time begin, cloud::time end = utils::time::null()) {
        using namespace vxg::cloud;
        std::promise<synchronizer::sync_request_status> p;

        segmented_uploader_->sync(
            begin, end,
            [&p](int progress, synchronizer::sync_request_status status) {
                if (status == synchronizer::sync_request_status::ERROR ||
                    status == synchronizer::sync_request_status::DONE)
                    p.set_value(status);
            });

        p.get_future().wait();
    }

    void switch_mode(upload_mode mode) {
        if (mode_ != mode) {
            logger->info("Switching operation mode {} -> {}.", mode_, mode);

            switch (mode) {
                case upload_mode::MODE_CONTINUOUS:
                case upload_mode::MODE_EVENT:
                    event_configs_.enabled = (mode == upload_mode::MODE_EVENT);
                    event_manager_->set_events(event_configs_);
                    _notify_continuos(mode == upload_mode::MODE_CONTINUOUS);
                    break;
                default:
                    logger->warn("Unknown mode {}", mode);
                    break;
            }
            mode_ = mode;
        }
    }
};
}  // namespace s3
}  // namespace cloud
}  // namespace vxg