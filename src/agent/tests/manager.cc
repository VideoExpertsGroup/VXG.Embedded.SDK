#include "manager.h"
#include <tests/test-helpers.h>

TEST_F(agent_manager_test, stream_record) {
    EXPECT_CALL(*source_, name()).Times(AnyNumber());
    EXPECT_CALL(*source_, pullFrame()).Times(AnyNumber());
    EXPECT_CALL(*sink_, name()).Times(AnyNumber());
    EXPECT_CALL(*sink_, negotiate(_)).Times(AnyNumber());

    EXPECT_CALL(*source_, init(_)).WillOnce(Return(true));
    EXPECT_CALL(*sink_, init(_)).WillRepeatedly(Return(true));
    START_STREAM(proto::stream_reason::SR_RECORD);

    EXPECT_CALL(*source_, finit()).WillOnce(Return());
    EXPECT_CALL(*sink_, finit()).WillRepeatedly(Return(true));
    STOP_STREAM(proto::stream_reason::SR_RECORD);

    EXPECT_NO_THROW();
}

TEST_F(agent_manager_test, stream_by_event_no_event) {
    // Status not NORMAL causes live streaming recording
    SET_MEMORYCARD_STATUS(proto::MCS_NONE);

    // Starting and stopping stream in record_by_event mode with no memorycard
    // and not emitting the event should not start streaming

    // TODO: specify memorycard_info
    START_STREAM(proto::stream_reason::SR_RECORD_BY_EVENT);
    STOP_STREAM(proto::stream_reason::SR_RECORD_BY_EVENT);

    EXPECT_NO_THROW();
}

TEST_F(agent_manager_test, stream_by_event) {
    // Status not NORMAL causes live streaming recording
    SET_MEMORYCARD_STATUS(proto::MCS_NONE);

    // Expect media source/sink activity
    EXPECT_CALL(*source_, name()).Times(AnyNumber());
    EXPECT_CALL(*source_, pullFrame()).Times(AnyNumber());
    EXPECT_CALL(*sink_, name()).Times(AnyNumber());
    EXPECT_CALL(*sink_, negotiate(_)).Times(1);
    EXPECT_CALL(*source_, init(_)).WillOnce(Return(true));
    EXPECT_CALL(*sink_, init(_)).WillRepeatedly(Return(true));

    START_STREAM(proto::stream_reason::SR_RECORD_BY_EVENT);

    // by_event stream start should emit record event with record_info on=true

    // Wait for record event with on=true
    EXPECT_CALL(*transport_, send(is_event("record")))
        .WillOnce(Invoke([&](const transport::Data& m) {
            proto::event_object e = json::parse(m);
            EXPECT_TRUE(e.event == proto::ET_RECORD);
            EXPECT_TRUE(e.record_info.on);
            return true;
        }))
        .RetiresOnSaturation();

    // Notify event start
    NOTIFY_EVENT_VIA_EVENT_STREAM(0, true);
    // Wait for 10 seconds emulating ongoing event
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // Wait for record event with on=false
    EXPECT_CALL(*transport_, send(is_event(json(proto::ET_RECORD))))
        .WillOnce(Invoke([&](const transport::Data& m) {
            proto::event_object e = json::parse(m);
            EXPECT_TRUE(e.event == proto::ET_RECORD);
            EXPECT_FALSE(e.record_info.on);
            return true;
        }));

    // Expect stream stop after event stop notification and post rec time
    EXPECT_CALL(*source_, finit()).WillOnce(Return());
    EXPECT_CALL(*sink_, finit()).WillRepeatedly(Return(true));

    NOTIFY_EVENT_VIA_EVENT_STREAM(0, false);
    // Wait for post rec time + 1 second and stop stream
    std::this_thread::sleep_for(post_rec_time_ + std::chrono::seconds(1));

    STOP_STREAM(proto::stream_reason::SR_RECORD_BY_EVENT);

    EXPECT_NO_THROW();
}

TEST_F(agent_manager_test, stream_by_event_switch) {
    // Status not NORMAL causes live streaming recording
    SET_MEMORYCARD_STATUS(proto::MCS_NONE);
    Sequence s;

    // Notify event start
    NOTIFY_EVENT_VIA_EVENT_STREAM(0, true);
    // Wait for 10 seconds emulating ongoing event
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // by_event stream start should emit record event with record_info on=true
    // Wait for record event with on=true
    EXPECT_CALL(*transport_, send(is_event("record")))
        .WillOnce(Invoke([&](const transport::Data& m) {
            proto::event_object e = json::parse(m);
            EXPECT_TRUE(e.event == proto::ET_RECORD);
            EXPECT_TRUE(e.record_info.on);

            // transport_->schedule_timed_cb([&](){
            //     source_->error(Streamer::E_FATAL);
            // }, 4000);

            return true;
        }))
        .RetiresOnSaturation();

    // Expect media source/sink activity
    EXPECT_CALL(*source_, name()).Times(AnyNumber());
    EXPECT_CALL(*source_, pullFrame()).Times(AnyNumber());
    EXPECT_CALL(*sink_, name()).Times(AnyNumber());
    EXPECT_CALL(*sink_, negotiate(_)).Times(1);
    EXPECT_CALL(*source_, init(_)).WillOnce(Return(true));
    EXPECT_CALL(*sink_, init(_)).WillRepeatedly(Return(true));

    // Switch recorded stream delivery mode NONE => BY_EVENT
    // Live stream delivery should be started
    START_STREAM(proto::stream_reason::SR_RECORD_BY_EVENT);

    // Wait for 5 seconds more
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Wait for record event with on=false
    EXPECT_CALL(*transport_, send(is_event(json(proto::ET_RECORD))))
        .WillOnce(Invoke([&](const transport::Data& m) {
            proto::event_object e = json::parse(m);
            EXPECT_TRUE(e.event == proto::ET_RECORD);
            EXPECT_FALSE(e.record_info.on);
            return true;
        }));

    // Expect stream stop after event stop notification and post rec time
    EXPECT_CALL(*source_, finit()).WillOnce(Return());
    EXPECT_CALL(*sink_, finit()).WillRepeatedly(Return(true));

    // Stop by event stream should stop records sync, i.e. we should receive
    // 'record' event with record_info.on==false
    STOP_STREAM(proto::stream_reason::SR_RECORD_BY_EVENT);
    std::this_thread::sleep_for(post_rec_time_ + std::chrono::seconds(3));

    // Stop event
    NOTIFY_EVENT_VIA_EVENT_STREAM(0, false);
    // Wait for post rec time + 1 second and stop stream
    std::this_thread::sleep_for(post_rec_time_ + std::chrono::seconds(1));

    EXPECT_NO_THROW();
}

TEST_F(agent_manager_test, stream_by_event_switch_memorycard) {
    // Status not NORMAL causes live streaming recording
    SET_MEMORYCARD_STATUS(proto::MCS_NORMAL);
    Sequence s;

    // Notify event start
    NOTIFY_EVENT_VIA_EVENT_STREAM(0, true);
    // Wait for 10 seconds emulating ongoing event
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Switch recorded stream delivery mode NONE => BY_EVENT
    // Direct upload stream delivery should be started

    // Test data
    // Local timeline: ---------now++++++++++++++now+25-------------
    // event:          --now-5s------now+10-------------------------
    // Mode switch     ----------X----------------------------------
    // Remote timeline:---------now+++++++++now+15------------------
    // Stop event +10 seconds
    // Floor time to seconds since VXG proto uses milliseconds precision
    auto modeswitch_time =
        date::floor<std::chrono::milliseconds>(utils::time::now());
    logger->info("NOW: {}", utils::time::to_iso_packed(modeswitch_time));
    transport_->schedule_timed_cb(
        [&]() {
            NOTIFY_EVENT_VIA_EVENT_STREAM(
                0, false, modeswitch_time + std::chrono::seconds(10));
        },
        10000);

    remote_timeline_ = SYNC_TEST(
        {{std::chrono::time_point_cast<std::chrono::seconds>(modeswitch_time)
              .time_since_epoch()
              .count(),
          std::chrono::time_point_cast<std::chrono::seconds>(modeswitch_time)
                  .time_since_epoch()
                  .count() +
              25}},
        {{0, 25, 0, sync_type::SYNC_LAZY}});
    remote_timeline_.squash_timeline();

    EXPECT_EQ(result_good_syncs_, 1);
    EXPECT_EQ(result_bad_syncs_, 0);
    EXPECT_EQ(remote_timeline_.size(), 1);
    EXPECT_TRUE((remote_timeline_.get(0).begin >=
                 (modeswitch_time - std::chrono::milliseconds(1000))) &&
                (remote_timeline_.get(0).begin <=
                 (modeswitch_time + std::chrono::milliseconds(1000))));
    EXPECT_EQ(remote_timeline_.get(0).end,
              modeswitch_time + std::chrono::seconds(10) + post_rec_time_);

    EXPECT_NO_THROW();
}

TEST_F(agent_manager_test, stream_by_event_intersecting_events) {
    // Status NORMAL causes recorded segments upload using
    // timeline::synchronizer
    SET_MEMORYCARD_STATUS(proto::MCS_NONE);

    // Expect media source/sink activity
    EXPECT_CALL(*source_, name()).Times(AnyNumber());
    EXPECT_CALL(*source_, pullFrame()).Times(AnyNumber());
    EXPECT_CALL(*sink_, name()).Times(AnyNumber());
    EXPECT_CALL(*sink_, negotiate(_)).Times(1);
    EXPECT_CALL(*source_, init(_)).WillOnce(Return(true));
    EXPECT_CALL(*sink_, init(_)).WillRepeatedly(Return(true));

    START_STREAM(proto::stream_reason::SR_RECORD_BY_EVENT);

    // by_event stream start should emit record event with record_info on=true

    // Wait for record event with on=true, should be started only once
    EXPECT_CALL(*transport_, send(is_event("record")))
        .WillOnce(Invoke([&](const transport::Data& m) {
            proto::event_object e = json::parse(m);
            EXPECT_TRUE(e.event == proto::ET_RECORD);
            EXPECT_TRUE(e.record_info.on);
            return true;
        }))
        .RetiresOnSaturation();

    // Notify event start
    NOTIFY_EVENT_VIA_EVENT_STREAM(0, true);
    // Wait for 5 seconds emulating ongoing event and start second event to
    // check intersecting events
    std::this_thread::sleep_for(std::chrono::seconds(5));
    NOTIFY_EVENT_VIA_EVENT_STREAM(1, true);
    // Wait for 5 more seconds
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Expect record event with on=false on last event stop time + post_rec_time
    // expired
    EXPECT_CALL(*transport_, send(is_event(json(proto::ET_RECORD))))
        .WillOnce(Invoke([&](const transport::Data& m) {
            proto::event_object e = json::parse(m);
            EXPECT_TRUE(e.event == proto::ET_RECORD);
            EXPECT_FALSE(e.record_info.on);

            // record event[on=false] time must be near event[active=false] time
            // + post rec time +- threshold
            auto expected_event_time = last_event_time_ + post_rec_time_;
            EXPECT_TRUE(
                (expected_event_time >= (utils::time::from_double(e.time) -
                                         std::chrono::milliseconds(1000))) &&
                (expected_event_time <= (utils::time::from_double(e.time) +
                                         std::chrono::milliseconds(1000))));
            return true;
        }));

    // Expect stream stop after event stop notification and post rec time
    EXPECT_CALL(*source_, finit()).WillOnce(Return());
    EXPECT_CALL(*sink_, finit()).WillRepeatedly(Return(true));

    NOTIFY_EVENT_VIA_EVENT_STREAM(0, false);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    NOTIFY_EVENT_VIA_EVENT_STREAM(1, false);

    // Wait for event's post rec time for actual stream stop
    std::this_thread::sleep_for(post_rec_time_ + std::chrono::seconds(1));

    STOP_STREAM(proto::stream_reason::SR_RECORD_BY_EVENT);

    EXPECT_NO_THROW();
}

void agent_manager_test::SETUP_SYNC_EXPECTATIONS(
    storage_stub& storage_timeline,
    storage_stub& remote_timeline) {
    std::vector<period> result_sync_chunks;  // not used

    // stream::record_get_list() stub
    EXPECT_CALL(*stream_, record_get_list(_, _, true))
        .WillRepeatedly(
            Invoke([&](vxg::cloud::time b, vxg::cloud::time e, bool a) {
                return storage_timeline.list(b, e);
            }));

    // stream::record_export() stub
    EXPECT_CALL(*stream_, record_export(_, _))
        .WillRepeatedly(Invoke([&](vxg::cloud::time b, vxg::cloud::time e) {
            proto::video_clip_info clip;

            // Always mock success export with 256 bytes of data
            clip.tp_start = b;
            clip.tp_stop = e;
            clip.data = std::vector<uint8_t>(256);

            // Save "exported" period for assert phase of the test
            // result_sync_chunks.push_back({b, e});

            return clip;
        }));

    // Expect get_direct_upload_url command and reply with direct_upload_url
    EXPECT_CALL(*transport_, send(is_command(proto::GET_DIRECT_UPLOAD_URL)))
        .WillRepeatedly(Invoke([&](const transport::Data& msg) {
            auto request =
                std::static_pointer_cast<proto::get_direct_upload_url>(
                    proto::command::factory::parse(json::parse(msg)));
            std::shared_ptr<proto::direct_upload_url> url =
                std::static_pointer_cast<proto::direct_upload_url>(
                    proto::command::factory::reply(request.get(),
                                                   proto::DIRECT_UPLOAD_URL));

            url->category = proto::UC_RECORD;
            url->url = "http://fake-url";
            url->status = "OK";

            logger->info("Remote timeline stored item {} duration {}ms",
                         request->file_time, request->duration);

            remote_timeline_.add(
                utils::time::from_iso_packed(request->file_time),
                utils::time::from_iso_packed(request->file_time) +
                    std::chrono::milliseconds(request->duration));

            transport_->INJECT_CMD_WAIT_FOR_MATCHED_REPLY_WITH_RESULT<
                command::direct_upload_url,
                command_replyMatcherP2<const char*, const char*>, proto::done>(
                *url, command_reply(command::DIRECT_UPLOAD_URL, command::DONE));
            return true;
        }));

    // Collect info from sync status callback, callback sets promise if it
    EXPECT_CALL(*manager_, sync_status_callback)
        .WillRepeatedly(
            Invoke([&](int progress, synchronizer::sync_request_status status,
                       synchronizer::segmenter_ptr seg) {
                logger->info("Segmenter[{}] status: {}, ticket: {}",
                             (void*)seg.get(), status, seg->ticket);

                switch (status) {
                    case synchronizer::sync_request_status::DONE:
                        result_good_syncs_++;
                        break;
                    case synchronizer::sync_request_status::ERROR:
                        result_bad_syncs_++;
                        break;
                    case synchronizer::sync_request_status::PENDING:
                        break;
                    case synchronizer::sync_request_status::CANCELED:
                        break;
                    default:
                        break;
                }

                // Unblock waiter's promise if callback called with final
                // status, progress should always be 100% for final status
                if (status == synchronizer::sync_request_status::DONE ||
                    status == synchronizer::sync_request_status::ERROR) {
                    EXPECT_EQ(progress, 100);

                    if (--expected_syncs_ == 0)
                        wait_promise_.set_value();
                }
            }));
}

agent_manager_test::storage_stub agent_manager_test::SYNC_TEST(
    std::vector<std::pair<size_t, size_t>> local_timeline_periods,
    std::vector<std::tuple<size_t, size_t, size_t, sync_type>> syncs) {
    storage_stub local_timeline("local");

    logger->info("Setting up expectations for timeline sync:");

    for (auto& p : local_timeline_periods) {
        local_timeline.add(cloud::time {std::chrono::seconds(p.first)},
                           cloud::time {std::chrono::seconds(p.second)});
    }

    // Status NORMAL causes recorded segments upload using
    // timeline::synchronizer
    SET_MEMORYCARD_STATUS(proto::MCS_NORMAL);

    // Expect media source/sink activity
    EXPECT_CALL(*stream_, start_record()).Times(1);
    EXPECT_CALL(*stream_, stop_record()).Times(1);

    // Start stream in record by event mode
    START_STREAM(proto::stream_reason::SR_RECORD_BY_EVENT);

    expected_syncs_ = syncs.size();
    // Agent manager's internals stubs setup
    SETUP_SYNC_EXPECTATIONS(local_timeline, remote_timeline_);

    // Emit sync requests, it may be events or memorycard_synchronize
    cloud::time epoch = utils::time::null();
    // Overall sync duration should be not more than:
    //      - event: num events * (event duration + pre/post duration + sync
    //      start delay + 1 second threshold)
    //      - memorycard_synchronize: num requests * 1 second threshold
    cloud::duration test_timeout = std::chrono::seconds(0);
    for (auto& s : syncs) {
        switch (std::get<3>(s)) {
            // Sync request using events
            case sync_type::SYNC_EVENT: {
                auto event_start = epoch + std::chrono::seconds(std::get<1>(s));
                auto event_stop = epoch + std::chrono::seconds(std::get<2>(s));
                auto event_id = std::get<0>(s);

                assert(event_id < MAX_EVENT_ID);

                // Notify event start/stop with predefined time
                NOTIFY_EVENT_VIA_EVENT_STREAM(event_id, true, event_start);
                NOTIFY_EVENT_VIA_EVENT_STREAM(event_id, false, event_stop);

                test_timeout +=
                    (event_stop - event_start) + pre_rec_time_ +
                    post_rec_time_ +
                    config_.delay_between_event_and_records_upload_start +
                    std::chrono::seconds(1);
            } break;
            // Sync request using events
            case sync_type::SYNC_EVENT_REALTIME: {
                auto event_offset_start = std::chrono::seconds(std::get<1>(s));
                auto event_offset_stop = std::chrono::seconds(std::get<2>(s));
                auto event_start = epoch + event_offset_start;
                auto event_stop = epoch + event_offset_stop;
                auto event_id = std::get<0>(s);

                assert(event_id < MAX_EVENT_ID);

                // Notify event start/stop with predefined time asynchronously
                realtime_timer_helper_.schedule_timed_cb(
                    [this, event_id, event_start]() {
                        NOTIFY_EVENT_VIA_EVENT_STREAM(event_id, true,
                                                      event_start);
                    },
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        event_offset_start)
                        .count());
                realtime_timer_helper_.schedule_timed_cb(
                    [this, event_id, event_stop]() {
                        NOTIFY_EVENT_VIA_EVENT_STREAM(event_id, false,
                                                      event_stop);
                    },
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        event_offset_stop)
                        .count());

                test_timeout +=
                    (event_stop - event_start) + pre_rec_time_ +
                    post_rec_time_ +
                    config_.delay_between_event_and_records_upload_start +
                    std::chrono::seconds(1);
            } break;
            // Sync request using memorycard_synchronize command
            case sync_type::SYNC_SYNC: {
                proto::cam_memorycard_synchronize sync_command;
                sync_command.request_id = utils::random_string(64);
                sync_command.start = utils::time::to_iso_packed(
                    epoch + std::chrono::seconds(std::get<1>(s)));
                sync_command.end = utils::time::to_iso_packed(
                    epoch + std::chrono::seconds(std::get<2>(s)));
                EXPECT_CALL(
                    *transport_,
                    send(is_command(proto::CAM_MEMORYCARD_SYNCHRONIZE_STATUS)))
                    .WillRepeatedly(Return(true));
                transport_->INJECT_COMMAND(sync_command);
                test_timeout += std::chrono::seconds(10);
            } break;
            // External sync request, we should do nothing but wait for
            // requested time
            case sync_type::SYNC_LAZY:
                test_timeout += std::chrono::seconds(std::get<1>(s));
                break;
            default:
                break;
        }
    }

    EXPECT_EQ(wait_promise_.get_future().wait_for(test_timeout),
              future_status::ready);

    // Stop stream in record by event mode
    STOP_STREAM(proto::stream_reason::SR_RECORD_BY_EVENT);

    return remote_timeline_;
}

TEST_F(agent_manager_test, stream_by_event_sync_1) {
    std::promise<void> prom;

    pre_rec_time_ = post_rec_time_ = std::chrono::seconds(5);

    // Local timeline:  -----------70++++++++++++++150-------------
    // Event:           ---------60++++++++++80--------------------
    // Remote timeline: -----------70+++++++++++85-----------------
    remote_timeline_ =
        SYNC_TEST({{70, 150}}, {{0, 60, 80, sync_type::SYNC_EVENT}});
    remote_timeline_.squash_timeline();

    EXPECT_EQ(result_good_syncs_, 1);
    EXPECT_EQ(result_bad_syncs_, 0);
    EXPECT_EQ(remote_timeline_.size(), 1);
    EXPECT_EQ(remote_timeline_.get(0).begin, _epoch + std::chrono::seconds(70));
    EXPECT_EQ(remote_timeline_.get(0).end, _epoch + std::chrono::seconds(85));
    EXPECT_NO_THROW();
}

TEST_F(agent_manager_test, stream_by_event_sync_2) {
    std::promise<void> prom;

    pre_rec_time_ = post_rec_time_ = std::chrono::seconds(5);

    // Local timeline:  -----------70++++++++++++++150-------------
    // Event:           -------------75++++++80--------------------
    // Remote timeline: -----------70++++++++++85------------------
    remote_timeline_ =
        SYNC_TEST({{70, 150}}, {{0, 75, 80, sync_type::SYNC_EVENT}});
    remote_timeline_.squash_timeline();

    EXPECT_EQ(result_good_syncs_, 1);
    EXPECT_EQ(result_bad_syncs_, 0);
    EXPECT_EQ(remote_timeline_.size(), 1);
    EXPECT_EQ(remote_timeline_.get(0).begin, _epoch + std::chrono::seconds(70));
    EXPECT_EQ(remote_timeline_.get(0).end, _epoch + std::chrono::seconds(85));
    EXPECT_NO_THROW();
}

TEST_F(agent_manager_test, stream_by_event_sync_3) {
    std::promise<void> prom;

    pre_rec_time_ = post_rec_time_ = std::chrono::seconds(5);

    // Local timeline:  -----------70++++++++++++++150-------------
    // Event1           ---------60+++++80-------------------------
    // Event2           -------------70+++++85-------------------------
    // Remote timeline: -----------70++++++++++90------------------
    remote_timeline_ =
        SYNC_TEST({{70, 150}}, {{0, 60, 80, sync_type::SYNC_EVENT},
                                {1, 70, 85, sync_type::SYNC_EVENT}});
    remote_timeline_.squash_timeline();

    EXPECT_EQ(result_good_syncs_, 2);
    EXPECT_EQ(result_bad_syncs_, 0);
    EXPECT_EQ(remote_timeline_.size(), 1);
    EXPECT_EQ(remote_timeline_.get(0).begin, _epoch + std::chrono::seconds(70));
    EXPECT_EQ(remote_timeline_.get(0).end, _epoch + std::chrono::seconds(90));
    EXPECT_NO_THROW();
}

TEST_F(agent_manager_test, stream_by_event_sync_4) {
    std::promise<void> prom;

    pre_rec_time_ = post_rec_time_ = std::chrono::seconds(5);

    // Local timeline:  -----------70++++++++++++++150-------------
    // event            -----60++65--------------------------------
    // event            ---------60+++++80-------------------------
    // sync_request     ---------60+++++80-------------------------
    // Remote timeline: -----------70+++++85-----------------------
    remote_timeline_ =
        SYNC_TEST({{70, 150}}, {{0, 60, 65, sync_type::SYNC_EVENT},
                                {1, 60, 80, sync_type::SYNC_EVENT},
                                {1, 60, 80, sync_type::SYNC_SYNC}});
    remote_timeline_.squash_timeline();

    EXPECT_EQ(result_good_syncs_, 2);
    EXPECT_EQ(result_bad_syncs_, 1);
    EXPECT_EQ(remote_timeline_.size(), 1);
    EXPECT_EQ(remote_timeline_.get(0).begin, _epoch + std::chrono::seconds(70));
    EXPECT_EQ(remote_timeline_.get(0).end, _epoch + std::chrono::seconds(85));
    EXPECT_NO_THROW();
}

TEST_F(agent_manager_test, stream_by_event_sync_5) {
    std::promise<void> prom;

    pre_rec_time_ = post_rec_time_ = std::chrono::seconds(5);

    // Local timeline:  -----------70++++++++++++++150-------------
    // sync_request     -----60++65--------------------------------
    // sync_request     ---------60+++++80-------------------------
    // sync_request     ---------60+++++80-------------------------
    // Remote timeline: ------------70++80-------------------------
    remote_timeline_ =
        SYNC_TEST({{70, 150}}, {{0, 60, 65, sync_type::SYNC_SYNC},
                                {1, 60, 80, sync_type::SYNC_SYNC},
                                {1, 60, 80, sync_type::SYNC_SYNC}});
    remote_timeline_.squash_timeline();

    EXPECT_EQ(result_good_syncs_, 2);
    EXPECT_EQ(result_bad_syncs_, 1);
    EXPECT_EQ(remote_timeline_.size(), 1);
    EXPECT_EQ(remote_timeline_.get(0).begin, _epoch + std::chrono::seconds(70));
    EXPECT_EQ(remote_timeline_.get(0).end, _epoch + std::chrono::seconds(80));
    EXPECT_NO_THROW();
}

TEST_F(agent_manager_test, stream_by_event_sync_6) {
    std::promise<void> prom;

    pre_rec_time_ = post_rec_time_ = std::chrono::seconds(5);

    // Local timeline:  -----------70++++++++++++++150-------------
    // event            -------60++65------------------------------
    // sync_request     ---------60+++++80-------------------------
    // event            ---------60+++++80-------------------------
    // Remote timeline: ------------70++++++85---------------------
    remote_timeline_ =
        SYNC_TEST({{70, 150}}, {{0, 60, 65, sync_type::SYNC_EVENT},
                                {1, 60, 80, sync_type::SYNC_SYNC},
                                {1, 60, 80, sync_type::SYNC_EVENT}});
    remote_timeline_.squash_timeline();

    EXPECT_EQ(result_good_syncs_, 2);
    EXPECT_EQ(result_bad_syncs_, 1);
    EXPECT_EQ(remote_timeline_.size(), 1);
    EXPECT_EQ(remote_timeline_.get(0).begin, _epoch + std::chrono::seconds(70));
    EXPECT_EQ(remote_timeline_.get(0).end, _epoch + std::chrono::seconds(85));
    EXPECT_NO_THROW();
}

TEST_F(agent_manager_test, stream_by_event_sync_7) {
    std::promise<void> prom;

    pre_rec_time_ = post_rec_time_ = std::chrono::seconds(5);

    // Local timeline: ---------70++++++++++++++150------------
    // event           -----60++65-----------------------------
    // sync_request    -----60++++++75-------------------------
    // event           ----------70+++++80---------------------
    // Remote timeline:----------70++++++++85------------------
    remote_timeline_ =
        SYNC_TEST({{70, 150}}, {{0, 60, 65, sync_type::SYNC_EVENT},
                                {1, 60, 80, sync_type::SYNC_SYNC},
                                {1, 60, 80, sync_type::SYNC_EVENT}});
    remote_timeline_.squash_timeline();

    EXPECT_EQ(result_good_syncs_, 2);
    EXPECT_EQ(result_bad_syncs_, 1);
    EXPECT_EQ(remote_timeline_.size(), 1);
    EXPECT_EQ(remote_timeline_.get(0).begin, _epoch + std::chrono::seconds(70));
    EXPECT_EQ(remote_timeline_.get(0).end, _epoch + std::chrono::seconds(85));
    EXPECT_NO_THROW();
}

TEST_F(agent_manager_test, stream_by_event_sync_8) {
    std::promise<void> prom;

    pre_rec_time_ = post_rec_time_ = std::chrono::seconds(5);

    // Local timeline: ---------70++++++++++++++150-----------------
    // event           ---------70++75------------------------------
    // event           ------------------80++++++85-----------------
    // event           ------------------------------90+++++95------
    // Remote timeline:---------70+++++++++++++++++++++++++++++100--
    remote_timeline_ =
        SYNC_TEST({{70, 150}}, {{0, 70, 75, sync_type::SYNC_EVENT},
                                {1, 80, 85, sync_type::SYNC_EVENT},
                                {2, 90, 95, sync_type::SYNC_EVENT}});
    remote_timeline_.squash_timeline();

    EXPECT_EQ(result_good_syncs_, 3);
    EXPECT_EQ(result_bad_syncs_, 0);
    EXPECT_EQ(remote_timeline_.size(), 1);
    EXPECT_EQ(remote_timeline_.get(0).begin, _epoch + std::chrono::seconds(70));
    EXPECT_EQ(remote_timeline_.get(0).end, _epoch + std::chrono::seconds(100));
    EXPECT_NO_THROW();
}

TEST_F(agent_manager_test, stream_by_event_sync_9) {
    // Local timeline: ---------70++++++++++++++150-----------------
    // sync_request    ---------70++75------------------------------
    // sync_request    ------------------80++++++85-----------------
    // sync_request    ------------------------------90+++++95------
    // Remote timeline:---------70++75---80++++++85--90+++++95------
    remote_timeline_ =
        SYNC_TEST({{70, 150}}, {{0, 70, 75, sync_type::SYNC_SYNC},
                                {0, 80, 85, sync_type::SYNC_SYNC},
                                {0, 90, 95, sync_type::SYNC_SYNC}});
    remote_timeline_.squash_timeline();

    EXPECT_EQ(result_good_syncs_, 3);
    EXPECT_EQ(result_bad_syncs_, 0);
    EXPECT_EQ(remote_timeline_.size(), 3);
    EXPECT_EQ(remote_timeline_.get(0).begin, _epoch + std::chrono::seconds(70));
    EXPECT_EQ(remote_timeline_.get(0).end, _epoch + std::chrono::seconds(75));
    EXPECT_EQ(remote_timeline_.get(1).begin, _epoch + std::chrono::seconds(80));
    EXPECT_EQ(remote_timeline_.get(1).end, _epoch + std::chrono::seconds(85));
    EXPECT_EQ(remote_timeline_.get(2).begin, _epoch + std::chrono::seconds(90));
    EXPECT_EQ(remote_timeline_.get(2).end, _epoch + std::chrono::seconds(95));
    EXPECT_NO_THROW();
}

// Multiple event start/stop in a row with intersections
TEST_F(agent_manager_test, stream_by_event_sync_10) {
    std::promise<void> prom;
    std::vector<std::tuple<size_t, size_t, size_t, sync_type>> event_list;
    size_t start = 10;
    size_t stop = 100;

    pre_rec_time_ = post_rec_time_ = std::chrono::seconds(5);

    // Local timeline: ---------10++++++++++++++++++++++++++++++100-
    // event0          ---------10+++10(+1~2s)----------------------
    // .............................................................
    // event99         ---------------------------------99+~(+1-2)--
    // Remote timeline:---------10++++++++++++++++++++++++++++++100-
    for (size_t t = start, d = (rand() % 3) + 1; t < stop;
         d = (rand() % 3) + 1) {
        logger->info("Adding realtime event {} - {}", t, t + d);
        event_list.push_back({0, t, t += d, sync_type::SYNC_EVENT_REALTIME});
    }
    remote_timeline_ = SYNC_TEST({{start, stop}}, event_list);
    remote_timeline_.squash_timeline();

    EXPECT_EQ(result_good_syncs_, event_list.size());
    EXPECT_EQ(result_bad_syncs_, 0);
    EXPECT_EQ(remote_timeline_.size(), 1);
    EXPECT_EQ(remote_timeline_.get(0).begin,
              _epoch + std::chrono::seconds(start));
    EXPECT_EQ(remote_timeline_.get(0).end, _epoch + std::chrono::seconds(stop));
    EXPECT_NO_THROW();
}