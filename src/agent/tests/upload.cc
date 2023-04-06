#include "upload.h"
#include <tests/test-helpers.h>

using namespace std::chrono;

TEST_F(uploader_test, Create) {
    // WAIT_FOR_CALL(*this, on_connected, this->transport_->connect(""));
    // sleep(5);
    EXPECT_NO_THROW();
}

TEST_F(uploader_test, SchedUpload_One_300s) {
    auto s = system_clock::now();

    SCHED_UPLOAD(s - seconds(300), s);

    EXPECT_FALSE(result_has_intersections());
    EXPECT_TRUE(wait_all_segments_finished());
    EXPECT_TRUE(result_continuos());

    EXPECT_NO_THROW();
}

TEST_F(uploader_test, SchedUpload_StopOnTheFly) {
    auto s = system_clock::now();

    SCHED_UPLOAD(s - seconds(300), s, seconds(30), seconds(30), false);

    // EXPECT_FALSE(wait_all_segments_finished(seconds(15)));

    EXPECT_NO_THROW();
}

void TEST_MULTI_OFFSETS(uploader_test* obj,
                        std::vector<std::vector<int>>& offsets,
                        seconds pre = seconds(0),
                        seconds post = seconds(0),
                        seconds timeout = seconds(30),
                        bool realtime = false) {
    auto now = time_point_cast<microseconds>(utils::time::now());
    std::vector<period> periods;

    for (auto& p : offsets)
        periods.push_back(period(now + seconds(p[0]), now + seconds(p[1])));

    obj->SCHED_UPLOAD_MULTIPLE(periods, pre, post, realtime);

    EXPECT_TRUE(obj->wait_all_segments_finished(timeout));

    auto a = obj->squash_uploads(periods);
    auto b = obj->squash_uploads(obj->q_);

    EXPECT_EQ(a.size(), b.size());

    for (size_t i = 0; i < std::min(a.size(), b.size()); i++) {
        vxg::logger::info("Verify result: {} - {} ? {} - {}",
                          utils::time::to_iso_packed(a[i].begin - pre),
                          utils::time::to_iso_packed(a[i].end + post),
                          utils::time::to_iso_packed(b[i].begin),
                          utils::time::to_iso_packed(b[i].end));

        EXPECT_EQ(a[i].begin - pre, b[i].begin);
        EXPECT_EQ(a[i].end + post, b[i].end);
    }
}

TEST_F(uploader_test, SchedUpload_Multiple_Intersecting_NoGaps) {
    std::vector<std::vector<int>> offsets = {
        {0, 10}, {4, 13}, {10, 20}, {16, 25}, {21, 30}};

    TEST_MULTI_OFFSETS(this, offsets);

    EXPECT_FALSE(result_has_intersections());
    EXPECT_TRUE(result_continuos());
    EXPECT_NO_THROW();
}

TEST_F(uploader_test, SchedUpload_Multiple_Intersecting_Gap) {
    std::vector<std::vector<int>> offsets = {{0, 10},  {11, 20}, {16, 25},
                                             {21, 30}, {33, 37}, {37, 47}};

    TEST_MULTI_OFFSETS(this, offsets);

    EXPECT_FALSE(result_has_intersections());
    EXPECT_TRUE(wait_all_segments_finished());
    EXPECT_FALSE(result_continuos());
    EXPECT_NO_THROW();
}

TEST_F(uploader_test, SchedUpload_Multiple_100) {
    std::vector<std::vector<int>> offsets;
    size_t num_syncs = 100;

    int start = 10, stop = 20;
    for (size_t i = 0; i < num_syncs; i++) {
        std::vector<int> p;
        p.push_back(start);
        p.push_back(stop);
        offsets.push_back(p);

        start += 5;
        stop += 5;
    }

    TEST_MULTI_OFFSETS(
        this, offsets, std::chrono::seconds(5), std::chrono::seconds(5),
        seconds(num_syncs) + profile::global::instance()
                                 .delay_between_event_and_records_upload_start,
        true);

    EXPECT_FALSE(result_has_intersections());
    EXPECT_TRUE(result_continuos());
    EXPECT_NO_THROW();
}

TEST_F(uploader_test, SchedUpload_Multiple_Intersecting_Shifted) {
    std::vector<std::vector<int>> offsets = {{0, 10}, {1, 11}, {2, 12},
                                             {3, 13}, {4, 14}, {5, 15}};
    TEST_MULTI_OFFSETS(this, offsets);

    EXPECT_TRUE(wait_all_segments_finished());
    EXPECT_FALSE(result_has_intersections());
    EXPECT_TRUE(result_continuos());
    EXPECT_NO_THROW();
}

TEST_F(uploader_test, SchedUpload_Multiple_Intersecting_Shifted_Delayed) {
    std::vector<std::vector<int>> offsets = {{0, 10}, {1, 11}, {2, 12},
                                             {3, 13}, {4, 14}, {5, 15}};
    TEST_MULTI_OFFSETS(this, offsets);

    EXPECT_TRUE(wait_all_segments_finished());
    EXPECT_FALSE(result_has_intersections());
    EXPECT_TRUE(result_continuos());
    EXPECT_NO_THROW();
}

struct upload_time_point {
    cloud::time tp {seconds(0)};
    bool is_start {false};
    cloud::duration wait {seconds(0)};
};

static std::vector<upload_time_point> _sort_pack_upload_times(
    std::vector<std::pair<std::string, std::string>> intervals) {
    std::vector<upload_time_point> result;

    for (size_t i = 0; i < intervals.size(); i++) {
        upload_time_point event_tp_start, event_tp_stop;
        auto start = utils::time::from_iso_packed(intervals[i].first);
        auto stop = utils::time::from_iso_packed(intervals[i].second);

        event_tp_start.tp = start;
        event_tp_start.is_start = true;
        event_tp_stop.tp = stop;

        result.push_back(event_tp_start);
        result.push_back(event_tp_stop);
    }

    std::sort(result.begin(), result.end(),
              [](const upload_time_point& l, const upload_time_point& r) {
                  return l.tp < r.tp;
              });

    for (size_t i = 0; i < result.size(); i++) {
        if (i < result.size() - 1)
            result[i].wait = result[i + 1].tp - result[i].tp;
    }

    vxg::logger::info("Prepared {} timepoints", result.size());

    return result;
};

TEST_F(uploader_test, SchedUpload_Multiple_Intersecting_RealData1) {
    std::vector<std::pair<std::string, std::string>> real_intervals = {
        {"20220221T100853.843738", "20220221T100902.350000"},
        {"20220221T101021.048000", "20220221T101041.048000"},
        {"20220221T101027.958000", "20220221T101047.958000"},
        {"20220221T101035.106000", "20220221T101055.106000"},
        {"20220221T101044.860999", "20220221T101104.860999"},
        {"20220221T101055.691000", "20220221T101115.691000"},
        {"20220221T101113.591000", "20220221T101141.798000"},
        {"20220221T101221.255000", "20220221T101241.255000"},
        {"20220221T101233.132000", "20220221T101253.132000"},
        {"20220221T101328.326000", "20220221T101348.326000"},
        {"20220221T101336.398000", "20220221T101356.398000"},
        {"20220221T101341.184000", "20220221T101401.184000"},
        {"20220221T101343.934000", "20220221T101403.934000"},
        {"20220221T101343.934000", "20220221T101403.934000"},
        {"20220221T101354.422000", "20220221T101414.422000"}};

    auto plans = _sort_pack_upload_times(real_intervals);
    profile::global::instance().record_by_event_upload_step =
        std::chrono::seconds(10);

    segmented_uploader::segmenter_ptr s;
    std::queue<segmented_uploader::segmenter_ptr> seg_fifo;
    for (size_t i = 0; i < plans.size(); i++) {
        if (plans[i].is_start) {
            s = SCHED_UPLOAD_START_EVENT(plans[i].tp, seconds(0), seconds(0));
            seg_fifo.push(s);
        } else {
            SCHED_UPLOAD_FINALIZE(seg_fifo.front(), plans[i].tp);
            seg_fifo.pop();
        }

        vxg::logger::info(
            "Wait for {:3f}s",
            (double)duration_cast<milliseconds>(plans[i].wait).count() /
                1000.0f);
        // this_thread::sleep_for(plans[i].wait);
    }

    EXPECT_TRUE(wait_all_segments_finished(duration_cast<seconds>(
        plans.back().tp - plans.front().tp + seconds(1))));

    std::vector<period> real_periods;
    for (auto& i : real_intervals) {
        real_periods.push_back(period(utils::time::from_iso_packed(i.first),
                                      utils::time::from_iso_packed(i.second)));
    }
    auto a = squash_uploads(real_periods);
    auto b = squash_uploads(q_);

    EXPECT_EQ(a.size(), b.size());

    for (size_t i = 0; i < std::min(a.size(), b.size()); i++) {
        vxg::logger::debug("Check {} - {} ? {} - {}",
                           utils::time::to_iso_packed(a[i].begin),
                           utils::time::to_iso_packed(a[i].end),
                           utils::time::to_iso_packed(b[i].begin),
                           utils::time::to_iso_packed(b[i].end));
        EXPECT_EQ(a[i].begin, b[i].begin);
        EXPECT_EQ(a[i].end, b[i].end);
    }

    EXPECT_FALSE(result_continuos());
    EXPECT_NO_THROW();
}

TEST_F(uploader_test, Bad_Segmenter) {
    using namespace std::chrono;

    auto s = std::make_shared<agent::segmented_uploader::segmenter>();
    s->step = profile::global::instance().record_by_event_upload_step;
    s->begin = utils::time::from_iso("2022-03-10T06:51:40.000000Z");
    s->end = utils::time::from_iso("2022-03-10T06:51:46.000000Z");
    s->pre = seconds(10);
    s->post = seconds(10);
    s->seg_provider = stream_;
    s->realtime = true;
    s->last_processed_time = utils::time::now();
    s->delay = seconds(1);
    planned_segmenters_.push_back(s);

    s->cur_seg_start = utils::time::from_iso("2022-03-10T06:51:56.000000Z");
    s->cur_seg_stop = utils::time::from_iso("2022-03-10T06:52:06.000000Z");

    uploader_->schedule_upload(s);

    EXPECT_TRUE(wait_all_segments_finished());
    EXPECT_NO_THROW();
}
