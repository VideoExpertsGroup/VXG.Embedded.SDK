#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <future>

#include <agent/manager.h>
#include <net/transport.h>
#include <utils/utils.h>

#include <tests/test_Transport.h>
#include <tests/test_media_stream.h>

using namespace ::testing;
using namespace std;

using namespace vxg::cloud::transport;
using namespace vxg::cloud::agent;
using namespace vxg::cloud::agent::proto;
using namespace vxg;

struct mock_uploader : public agent::segmented_uploader {
    transport::libwebsockets::http::ptr transport_;

    static proto::access_token::ptr __gen_dummy_token() {
        return make_shared<proto::access_token>();
    }

    using _send_command_cb =
        class std::function<bool(const nlohmann::basic_json<>&)>;
    using _direct_upload_cb =
        class std::function<bool(command::get_direct_upload_url)>;

    mock_uploader(transport::libwebsockets::http::ptr transport,
                  _direct_upload_cb direct_upload_cb,
                  _send_command_cb send_command_cb)
        : agent::segmented_uploader(__gen_dummy_token(),
                                    transport,
                                    direct_upload_cb,
                                    send_command_cb),
          transport_ {transport} {}

    virtual ~mock_uploader() {}

    static std::shared_ptr<mock_uploader> create(
        std::shared_ptr<MockHTTPTransport> transport,
        _direct_upload_cb direct_upload_cb,
        _send_command_cb send_command_cb) {
        return std::make_shared<mock_uploader>(transport, direct_upload_cb,
                                               send_command_cb);
    }
};

class uploader_test : public Test {
public:
    agent::segmented_uploader::ptr uploader_;
    std::shared_ptr<MockHTTPTransport> transport_;
    std::vector<period> q_;
    agent::media::stream::ptr stream_;
    std::vector<agent::segmented_uploader::segmenter_ptr> planned_segmenters_;

    bool __on_direct_upload_req(command::get_direct_upload_url d) {
        cloud::time begin = utils::time::from_iso_packed(d.file_time);
        // Use microseconds duration if presented for for accurate export,
        // default duration in milliseconds floors the real value of duration.
        cloud::duration duration = (d.duration_us == cloud::duration(0))
                                       ? std::chrono::milliseconds(d.duration)
                                       : d.duration_us;
        cloud::time end = begin + duration;

        vxg::logger::info("Request direct upload for {} - {}",
                          utils::time::to_iso_packed(begin),
                          utils::time::to_iso_packed(end));

        d.on_finished(true);

        q_.push_back(period(d));
        return true;
    }

    bool __on_event_req(const nlohmann::basic_json<>& j) {
        vxg::logger::info("Event send request: {}", j.dump());
        return true;
    }

    std::vector<period> squash_uploads(std::vector<period> q) {
        using namespace utils::time;
        std::vector<period> result;

        if (!q.empty()) {
            // Sort by begining of the periods

            std::sort(q.begin(), q.end());

            // Merge all intersecting periods
            auto it = q.begin();
            period current(*it);
            it++;
            while (it != q.end()) {
                if (current.end >= it->begin)
                    current.end = std::max(current.end, it->end);
                else {
                    result.push_back(current);
                    current = period(*it);
                }
                it++;
            }

            result.push_back(current);
        }

        return result;
    }

    bool result_has_intersections() {
        bool result = false;
        std::sort(q_.begin(), q_.end());

        for (size_t i = 0; q_.size() && i < q_.size() - 1; i++) {
            if (period(q_[i]).intersects(period(q_[i + 1]))) {
                result = true;
                break;
            }
        }

        return result;
    }

    bool result_continuos() { return (squash_uploads(q_).size() == 1); }

    bool all_segmenters_finished() {
        size_t i = 0;
        for (;
             i < planned_segmenters_.size() && planned_segmenters_[i]->finished;
             i++)
            ;

        return i == planned_segmenters_.size();
    }

    bool wait_all_segments_finished(
        std::chrono::seconds timeout = std::chrono::seconds(10)) {
        std::chrono::seconds elapsed = std::chrono::seconds(0);
        bool finished = false;

        while (!(finished = all_segmenters_finished()) && elapsed < timeout) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            elapsed += std::chrono::seconds(1);
        }

        vxg::logger::info("Planned chunks:");
        for (auto& p : q_) {
            vxg::logger::info("{} - {}",
                              utils::time::to_iso_packed(period(p).begin),
                              utils::time::to_iso_packed(period(p).end));
        }
        vxg::logger::info("Squashed planned chunks:");
        for (auto& p : squash_uploads(q_)) {
            vxg::logger::info("{} - {}", utils::time::to_iso_packed(p.begin),
                              utils::time::to_iso_packed(p.end));
        }

        return finished;
    }

    //! @brief media::stream::get_records_list() stub.
    //!
    //! @param begin
    //! @param end
    //! @param align
    //! @return Always return one clip with requested begin, end time.
    std::vector<vxg::cloud::agent::proto::video_clip_info>
    __get_records_list(cloud::time begin, cloud::time end, bool align) {
        std::vector<vxg::cloud::agent::proto::video_clip_info> res;
        vxg::cloud::agent::proto::video_clip_info clip;

        clip.tp_start = begin;
        clip.tp_stop = end;
        res.push_back(clip);

        return res;
    }

    vxg::cloud::agent::proto::video_clip_info __record_export(cloud::time begin,
                                                              cloud::time end) {
        vxg::cloud::agent::proto::video_clip_info clip;

        clip.tp_start = begin;
        clip.tp_stop = end;
        clip.data = {0, 0, 0};

        return clip;
    }

    uploader_test() {}
    virtual ~uploader_test() {}

    virtual void SetUp() {
        profile::global::instance().record_by_event_upload_step =
            std::chrono::seconds(5);

        transport_ = std::make_shared<MockHTTPTransport>();

        stream_ = std::make_shared<mock_media_stream>("test-stream");
        uploader_ = mock_uploader::create(
            transport_,
            std::bind(&uploader_test::__on_direct_upload_req, this,
                      placeholders::_1),
            std::bind(&uploader_test::__on_event_req, this, placeholders::_1));

        // Always return one clip with requested begin, end time
        ON_CALL(*dynamic_pointer_cast<mock_media_stream>(this->stream_),
                record_get_list(_, _, _))
            .WillByDefault(Invoke(std::bind(
                &uploader_test::__get_records_list, this, placeholders::_1,
                placeholders::_2, placeholders::_3)));
        EXPECT_CALL(*dynamic_pointer_cast<mock_media_stream>(this->stream_),
                    record_get_list(_, _, _))
            .Times(AnyNumber());

        ON_CALL(*dynamic_pointer_cast<mock_media_stream>(this->stream_),
                record_export(_, _))
            .WillByDefault(
                Invoke(std::bind(&uploader_test::__record_export, this,
                                 placeholders::_1, placeholders::_2)));

        ON_CALL(*transport_, make(_)).WillByDefault(::testing::Return(true));
        EXPECT_CALL(*transport_, make(_)).Times(AnyNumber());

        ON_CALL(*transport_, make_blocked(_))
            .WillByDefault(::testing::Return(true));
        EXPECT_CALL(*transport_, make_blocked(_)).Times(AnyNumber());

        uploader_->start();
    }
    virtual void TearDown() {
        EXPECT_FALSE(result_has_intersections());

        uploader_->stop();
        transport_->stop();
    }

    agent::segmented_uploader::segmenter_ptr SCHED_UPLOAD(
        vxg::cloud::time b,
        vxg::cloud::time e = utils::time::null(),
        vxg::cloud::duration pre = std::chrono::seconds(5),
        vxg::cloud::duration post = std::chrono::seconds(5),
        bool realtime = false) {
        auto s = std::make_shared<agent::segmented_uploader::segmenter>();
        s->step = profile::global::instance().record_by_event_upload_step;
        s->begin = b;
        s->end = e;
        s->cur_seg_start = b - pre;
        s->cur_seg_stop = std::min(
            e != utils::time::null() ? e + post : utils::time::max(),
            b + profile::global::instance().record_by_event_upload_step);
        s->pre = pre;
        s->post = post;
        s->seg_provider = stream_;
        s->realtime = realtime;
        s->last_processed_time = utils::time::now();
        s->delay =
            realtime ? std::chrono::seconds(10) : std::chrono::seconds(0);
        planned_segmenters_.push_back(s);
        uploader_->schedule_upload(s);

        return s;
    }

    void SCHED_UPLOAD_FINALIZE(agent::segmented_uploader::segmenter_ptr s,
                               vxg::cloud::time b) {
        uploader_->finalize_upload(s, b);
    }

    agent::segmented_uploader::segmenter_ptr SCHED_UPLOAD_START_EVENT(
        vxg::cloud::time b,
        vxg::cloud::duration pre = std::chrono::seconds(5),
        vxg::cloud::duration post = std::chrono::seconds(5)) {
        return SCHED_UPLOAD(b, utils::time::null(), pre, post, true);
    }

    void SCHED_UPLOAD_STOP_EVENT(agent::segmented_uploader::segmenter_ptr s,
                                 vxg::cloud::time e) {
        SCHED_UPLOAD_FINALIZE(s, e);
    }

    void SCHED_UPLOAD_MULTIPLE(
        std::vector<period> periods,
        std::chrono::seconds pre = std::chrono::seconds(0),
        std::chrono::seconds post = std::chrono::seconds(0),
        bool realtime = false) {
        for (auto& p : periods) {
            SCHED_UPLOAD(p.begin, p.end, pre, post, realtime);
        }
    }
};