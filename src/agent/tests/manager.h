#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <future>

#include <agent/manager.h>
#include <net/transport.h>

#include <agent-proto/tests/test-command-handler.h>
#include <agent-proto/tests/test-command-matchers.h>
#include <agent/tests/agent-callback.h>
#include <agent/tests/media-stream.h>
#include <tests/test_Transport.h>

// date::floor()
#include <date/date.h>

#include "event-manager.h"

using namespace ::testing;
using namespace std;

using namespace vxg::cloud::transport;
using namespace vxg::cloud::agent;
using namespace vxg::cloud::agent::proto;
using namespace vxg::media;

const std::string TEST_ACCESS_TOKEN =
    "eyJjYW1pZCI6IDI0NjI4MywgImNtbmdyaWQiOiAyNDY3MjAsICJhY2Nlc3MiOiAiYWxsIiwgIn"
    "Rva2VuIjogInNoYXJlLmV5SnphU0k2SURJek1Ea3dPSDAuNjE4ZTI5NTB0YmMxZmFiMDAuVXg2"
    "SGtUYlExR05MZlZRR3E3LURoWDRFUWpNIiwgImFwaSI6ICJ3ZWIuc2t5dnIudmlkZW9leHBlcn"
    "RzZ3JvdXAuY29tIiwgImNhbSI6ICJjYW0uc2t5dnIudmlkZW9leHBlcnRzZ3JvdXAuY29tIn0"
    "=";

class mock_agent_manager : public agent::manager {
public:
    mock_agent_manager(const agent::config& config,
                       const proto::access_token& access_token,
                       agent::callback::ptr callback,
                       transport::worker::ptr transport,
                       transport::worker::ptr http_transport)
        : agent::manager(
              config,
              access_token,
              std::move(callback),
              transport,
              std::dynamic_pointer_cast<transport::libwebsockets::http>(
                  http_transport)) {}
    virtual ~mock_agent_manager() { ; }

    static std::shared_ptr<mock_agent_manager> create(
        const agent::config& config,
        agent::callback::ptr callback,
        mock_media_stream_ptr stream,
        mock_event_stream_ptr event_stream,
        transport::worker::ptr transport,
        transport::worker::ptr http_transport,
        const proto::access_token& access_token) {
        auto result = std::make_shared<mock_agent_manager>(
            config, access_token, std::move(callback), transport,
            http_transport);
        result->set_streams({stream});
        result->set_event_streams({event_stream});
        return result;
    }

    MOCK_METHOD(void,
                sync_status_callback,
                (int progress,
                 synchronizer::sync_request_status status,
                 synchronizer::segmenter_ptr seg),
                (override));

    MOCK_METHOD(void,
                on_event_continue,
                (const event_state&, const cloud::time&),
                (override));
};
using mock_agent_manager_ptr = std::shared_ptr<mock_agent_manager>;

struct agent_manager_test : public Test {
    vxg::logger::logger_ptr logger =
        vxg::logger::instance("agent-manager-test");

    proto::supported_streams_config supported_stream_;
    proto::stream_caps stream_caps_;
    mock_agent_callback_ptr callback_;
    mock_agent_manager_ptr manager_;
    agent::proto::access_token token_;
    mock_websock_transport_ptr transport_;
    mock_http_transport_ptr http_transport_;
    mock_media_stream_ptr stream_;
    mock_media_source_ptr source_;
    mock_media_sink_ptr sink_;
    mock_event_stream_ptr event_stream_;
    agent::proto::event_config stateful_event_conf_1_;
    agent::proto::event_config stateful_event_conf_2_;
    agent::proto::event_config stateful_event_conf_3_;

    agent::proto::events_config events_config_;
    std::chrono::milliseconds pre_rec_time_ {5000};
    std::chrono::milliseconds post_rec_time_ {5000};
    proto::memorycard_status memorycard_status_;
    cloud::time last_event_time_;
    agent::config config_;

    cloud::time NOTIFY_EVENT_VIA_EVENT_STREAM(
        size_t id,
        bool active,
        cloud::time t =
            date::floor<std::chrono::milliseconds>(utils::time::now())) {
        last_event_time_ = t;
        proto::cam_event e;
        e.event = events_config_.events[id].event;
        e.custom_event_name = events_config_.events[id].custom_event_name;
        e.active = active;
        e.time = utils::time::to_double(last_event_time_);
        event_stream_->notify(e);
        return last_event_time_;
    }

    void SET_MEMORYCARD_STATUS(proto::memorycard_status status) {
        logger->info("Set memory card status to {}", json(status));
        memorycard_status_ = status;
    }

    class storage_stub {
        std::string name_;
        std::vector<period> q;
        void __sort() { std::sort(q.begin(), q.end()); }

    public:
        storage_stub(std::string n) : name_ {n} {}

        void squash_timeline() {
            using namespace utils::time;
            std::vector<period> result;

            if (!q.empty()) {
                // Sort by beginning of the periods
                std::sort(q.begin(), q.end());

                // Merge all intersecting periods
                auto it = q.begin();
                period current(*it);
                it++;
                while (it != q.end()) {
                    // Chunks should not intersect
                    EXPECT_FALSE(current.intersects(*it));
                    if (current.intersects(*it)) {
                        vxg::logger::error(
                            "Intersection {} - {} with {} - {}",
                            utils::time::to_iso_packed(current.begin),
                            utils::time::to_iso_packed(current.end),
                            utils::time::to_iso_packed(it->begin),
                            utils::time::to_iso_packed(it->end));
                    }
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

            for (auto& s : q) {
                vxg::logger::info("BEFORE SQUASH {}: {} - {}", name_,
                                  utils::time::to_iso_packed(s.begin),
                                  utils::time::to_iso_packed(s.end));
            }

            q = result;

            for (auto& s : q) {
                vxg::logger::info("AFTER SQUASH {}: {} - {}", name_,
                                  utils::time::to_iso_packed(s.begin),
                                  utils::time::to_iso_packed(s.end));
            }
        }

        std::vector<proto::video_clip_info> list(cloud::time b, cloud::time e) {
            period req(b, e);
            std::vector<proto::video_clip_info> result;

            __sort();
            vxg::logger::info("List stub storage {}: {} - {} with {} items",
                              name_, utils::time::to_iso(b),
                              utils::time::to_iso(e), q.size());

            for (auto& p : q) {
                proto::video_clip_info clip;

                vxg::logger::info("List stub storage {} check {} - {}", name_,
                                  utils::time::to_iso(p.begin),
                                  utils::time::to_iso(p.end));
                // If requested period intersects with test storage period
                // we align requested period to test storage
                if (req.intersects(p)) {
                    clip.tp_start = (b < p.begin ? p.begin : b);
                    clip.tp_stop = (e > p.end ? p.end : e);
                    result.push_back(clip);
                }
            }

            return result;
        }

        void add(cloud::time b, cloud::time e) {
            vxg::logger::info("Add item to stub storage {}: {} - {}", name_,
                              utils::time::to_iso(b), utils::time::to_iso(e));

            q.push_back(period {b, e});
            __sort();
        }

        period& get(size_t i) { return q[i]; }

        size_t size() const { return q.size(); }
    };
    storage_stub remote_timeline_;

    void SETUP_SYNC_EXPECTATIONS(storage_stub& storage_timeline,
                                 storage_stub& remote_timeline);
    enum class sync_type {
        SYNC_NONE,
        SYNC_EVENT,
        SYNC_SYNC,
        SYNC_LAZY,
        SYNC_EVENT_REALTIME
    };
    storage_stub SYNC_TEST(
        std::vector<std::pair<size_t, size_t>> local_timeline_periods,
        std::vector<std::tuple<size_t, size_t, size_t, sync_type>> syncs);

    const size_t MAX_EVENT_ID = 3;
    const cloud::time _epoch = utils::time::epoch();
    std::promise<void> wait_promise_;
    size_t result_good_syncs_ = 0;
    size_t result_bad_syncs_ = 0;
    size_t expected_syncs_ = 0;
    transport::libwebsockets::http realtime_timer_helper_;

protected:
    agent_manager_test() : remote_timeline_ {"remote"} { ; }
    virtual ~agent_manager_test() { ; }

    virtual void SetUp() {
        realtime_timer_helper_.start();
        callback_ = std::make_unique<mock_agent_callback>();
        transport_ = std::make_shared<mock_websock_transport>();
        http_transport_ = std::make_shared<mock_http_transport>();
        token_ = proto::access_token::parse(TEST_ACCESS_TOKEN);
        source_ = std::make_shared<mock_media_source>();
        sink_ = std::make_shared<mock_media_sink>();
        stream_ =
            std::make_shared<mock_media_stream>("test-stream", source_, sink_);

        ON_CALL(*source_, name()).WillByDefault(Return("test-source"));
        ON_CALL(*source_, pullFrame()).WillByDefault(Invoke([]() {
            // Sleep prevents brutal busy looping
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return nullptr;
        }));

        ON_CALL(*sink_, name()).WillByDefault(Return("test-sink"));
        ON_CALL(*sink_, negotiate(_)).WillByDefault(Return(true));

        ON_CALL(*stream_, start_record()).WillByDefault(Return(true));
        ON_CALL(*stream_, stop_record()).WillByDefault(Return(true));

        // Memory card info callback, used in by_event recording to switch
        // between recorded file segments upload and live streaming recording
        EXPECT_CALL(*callback_, on_get_memorycard_info(_))
            .Times(AnyNumber())
            .WillRepeatedly(Invoke(
                [this](proto::event_object::memorycard_info_object& info) {
                    info.status = memorycard_status_;
                    return (memorycard_status_ == proto::MCS_NORMAL);
                }));
        // Called once on reconnect
        EXPECT_CALL(*callback_, on_bye(_)).WillOnce(Return());

        // Event stream events settings and default expectations
        // stateful snapshot disabled
        stateful_event_conf_1_.event = proto::ET_CUSTOM;
        stateful_event_conf_1_.custom_event_name = "test-event-stateful-1";
        stateful_event_conf_1_.active = true;
        stateful_event_conf_1_.caps.stateful = true;
        stateful_event_conf_1_.caps.stream = true;
        stateful_event_conf_1_.stream = true;
        stateful_event_conf_1_.caps.state_emulation_report_delay =
            std::chrono::seconds(1);
        // actual disabling, still presented in caps
        stateful_event_conf_1_.snapshot = false;
        stateful_event_conf_1_.caps.snapshot = true;

        stateful_event_conf_2_ = stateful_event_conf_1_;
        stateful_event_conf_2_.custom_event_name = "test-event-stateful-2";
        stateful_event_conf_3_ = stateful_event_conf_1_;
        stateful_event_conf_3_.custom_event_name = "test-event-stateful-3";
        // events config
        events_config_.enabled = true;
        events_config_.events = {stateful_event_conf_1_, stateful_event_conf_2_,
                                 stateful_event_conf_3_};
        event_stream_ =
            std::make_shared<mock_event_stream>("test-event-stream");
        // Invoke get_events callback
        EXPECT_CALL(*event_stream_, get_events)
            .WillOnce([&](std::vector<proto::event_config>& config) {
                config = events_config_.events;
                return true;
            });
        EXPECT_CALL(*event_stream_, start()).WillOnce(Return(true));
        EXPECT_CALL(*event_stream_, stop()).WillRepeatedly(Return());
        EXPECT_CALL(*event_stream_,
                    set_trigger_recording(_, pre_rec_time_.count(),
                                          post_rec_time_.count()))
            .WillRepeatedly(Return(true));

        // Always expect multiple cam_event commands for our test events
        EXPECT_CALL(*transport_, send(is_event(stateful_event_conf_1_.name())))
            .WillRepeatedly(Return(true));
        EXPECT_CALL(*transport_, send(is_event(stateful_event_conf_2_.name())))
            .WillRepeatedly(Return(true));
        EXPECT_CALL(*transport_, send(is_event(stateful_event_conf_3_.name())))
            .WillRepeatedly(Return(true));
        EXPECT_CALL(*transport_, send(is_event(json(proto::ET_MEMORYCARD))))
            .WillRepeatedly(Return(true));

        // Chunk duration and after event upload start delay is 5 seconds for
        // test speed increasing
        config_.record_by_event_upload_step =
            config_.delay_between_event_and_records_upload_start =
                std::chrono::seconds(5);

        // agent::manager mock object
        manager_ = mock_agent_manager::create(
            config_, std::move(callback_), stream_, event_stream_, transport_,
            http_transport_, token_);

        // Update transport::handle_() cb before calling manager::start() in
        // order to intercept commands with mock_websock_transport
        // NOTE: This refs the manager_ object and creates cyclic refs between
        // manager_ and transport_, we need to clear transport::handle_ callback
        // on stop to prevent objects leaking
        transport_->set_handle_cb(std::bind(&mock_agent_manager::on_receive,
                                            manager_, std::placeholders::_1));

        EXPECT_CALL(*transport_, is_secure()).WillRepeatedly(Return(true));

        EXPECT_CALL(*http_transport_, make(_))
            .WillRepeatedly(
                Invoke([&](transport::libwebsockets::http::request_ptr req) {
                    auto data = transport::Data("TEST RESPONSE CONTENT");
                    req->status_ = 200;
                    req->response_(data, req->status_);
                    return true;
                }));
        EXPECT_CALL(*http_transport_, make_blocked(_))
            .WillRepeatedly(
                // Mocking 'api/v2/storage/data/' response with data from
                // remote_timeline_
                Invoke([&](transport::libwebsockets::http::request_ptr req) {
                    auto data = transport::Data("TEST RESPONSE CONTENT");
                    utils::uri parsed_uri;
                    if (utils::uri::parse(req->url_, parsed_uri) &&
                        (parsed_uri.path + "?") ==
                            api::v2::storage::data::ENDPOINT) {
                        auto parts = utils::string_split(parsed_uri.query, '&');
                        cloud::time start, stop;

                        for (auto& p : parts) {
                            if (utils::string_startswith(p, "start="))
                                start = utils::time::from_iso2(
                                    &p[sizeof("start=") - 1]);
                            if (utils::string_startswith(p, "end="))
                                stop = utils::time::from_iso2(
                                    &p[sizeof("end=") - 1]);
                        }

                        auto uploaded_chunks =
                            remote_timeline_.list(start, stop);
                        json formatted_response;
                        formatted_response["objects"] = json::array();
                        for (auto& c : uploaded_chunks) {
                            logger->info("Remote storage chunk: {} - {}",
                                         utils::time::to_iso(c.tp_start),
                                         utils::time::to_iso(c.tp_stop));

                            json item;
                            item["start"] = utils::time::to_iso(c.tp_start);
                            item["end"] = utils::time::to_iso(c.tp_stop);
                            formatted_response["objects"] += item;
                        }

                        req->response_body_ = formatted_response.dump(2);

                        logger->info("v2::storage::data[{}] response:\n{}",
                                     parsed_uri.path + "?" + parsed_uri.query,
                                     req->response_body_);
                    }
                    req->status_ = 200;

                    if (req->response_)
                        req->response_(data, req->status_);
                    return true;
                }));

        EXPECT_CALL(*manager_, on_event_continue(_, _))
            .WillRepeatedly(Return());

        // Initial command proto ping-pong sequence
        // Mocking register, get_events, get_supported_streams and set_by_event
        // settings
        {
            // InSequence s;
            // Wait for first command agent sends on connect which should be
            // register
            {
                std::promise<void> prom_register_command;

                // Mock connect, send configure with new host, send bye with
                // reason BR_RECONNECT, expect disconnect() and finally connect
                // with newly configured host
                EXPECT_CALL(
                    *transport_,
                    connect(is_string_contains(DEFAULT_CLOUD_TOKEN_CAM)))
                    .WillOnce(Invoke([&](std::string url) {
                        transport_->on_connected();

                        // Send configure command with new host to reconnect on
                        // the first connect(), wait for configure done reply
                        {
                            proto::command::configure reconnect_configure;
                            reconnect_configure.server =
                                "test-reconnection-host";
                            INJECT_CMD_WAIT_FOR_MATCHED_REPLY(
                                transport_, reconnect_configure,
                                command_reply_done_OK(command::CONFIGURE));
                        }

                        // Inject bye command with BR_RECONNECT reason
                        {
                            command::bye bye;
                            bye.reason = proto::command::BR_RECONNECT;
                            transport_->INJECT_COMMAND(bye);
                        }

                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        // Emulate disconnect
                        transport_->on_disconnected();

                        return true;
                    }))
                    .RetiresOnSaturation();

                // Expect register command after connection
                EXPECT_CALL(*transport_, send(CommandRegisterOK()))
                    .WillRepeatedly(Invoke(
                        [&prom_register_command, this](
                            const transport::Data& message) { return true; }));
                // Wait for disconnect() due to bye/on_disconnected()
                std::promise<void> prom_disconnect;
                EXPECT_CALL(*transport_, disconnect())
                    // .WillRepeatedly(Return(true))
                    .WillOnce(Invoke([&prom_disconnect]() {
                        prom_disconnect.set_value();
                        return true;
                    }))
                    .RetiresOnSaturation();

                // Start manager, triggers transport's connect() mocked above,
                // reconnection below and register command further
                manager_->start();

                EXPECT_EQ(std::future_status::ready,
                          prom_disconnect.get_future().wait_for(
                              COMMAND_REPLY_TIMEOUT_SEC));

                // Expect connect() to configured above reconnection
                // hostname
                std::promise<void> prom_reconnect;
                EXPECT_CALL(
                    *transport_,
                    connect(is_string_contains("test-reconnection-host")))
                    .WillOnce(Invoke([&](std::string url) {
                        transport_->on_connected();
                        prom_reconnect.set_value();
                        return true;
                    }));
                EXPECT_EQ(std::future_status::ready,
                          prom_reconnect.get_future().wait_for(
                              std::chrono::seconds(10)));

                // EXPECT_EQ(std::future_status::ready,
                //           prom_register_command.get_future().wait_for(
                //               std::chrono::seconds(3)));
            }
            EXPECT_CALL(*transport_, disconnect()).WillRepeatedly(Return(true));

            // configure -> done
            INJECT_CMD_WAIT_FOR_MATCHED_REPLY(
                transport_, configure(),
                command_reply_done_OK(command::CONFIGURE));

            // hello -> done
            EXPECT_CALL(*transport_,
                        send(command_reply_done_OK(command::HELLO)))
                .WillOnce(Return(true));
            // 2nd hello -> cam_register
            INJECT_CMD_WAIT_FOR_MATCHED_REPLY(
                transport_, hello(), is_command(command::CAM_REGISTER));

            // get_cam_status -> cam_status
            INJECT_CMD_WAIT_FOR_MATCHED_REPLY(
                transport_, get_cam_status(),
                command_reply(command::GET_CAM_STATUS, command::CAM_STATUS));

            // get_supported_streams -> supported_streams_config
            EXPECT_CALL(*stream_, get_supported_stream(_));
            supported_stream_ =
                transport_->INJECT_CMD_WAIT_FOR_MATCHED_REPLY_WITH_RESULT<
                    command::get_supported_streams,
                    command_replyMatcherP2<const char*, const char*>,
                    proto::supported_streams_config>(
                    command::get_supported_streams(),
                    command_reply(command::GET_SUPPORTED_STREAMS,
                                  command::SUPPORTED_STREAMS));

            // get_stream_caps -> stream_caps
            EXPECT_CALL(*stream_, get_stream_caps(_));
            get_stream_caps _get_stream_caps;
            _get_stream_caps.video_es = {};
            _get_stream_caps.video_es.push_back(supported_stream_.video_es[0]);
            if (!supported_stream_.audio_es.empty()) {
                _get_stream_caps.audio_es = {};
                _get_stream_caps.audio_es.push_back(
                    supported_stream_.audio_es[0]);
            }

            stream_caps_ =
                transport_->INJECT_CMD_WAIT_FOR_MATCHED_REPLY_WITH_RESULT<
                    get_stream_caps,
                    command_replyMatcherP2<const char*, const char*>,
                    proto::stream_caps>(_get_stream_caps,
                                        command_reply(command::GET_STREAM_CAPS,
                                                      command::STREAM_CAPS));

            // set_stream_by_event -> done
            set_stream_by_event stream_by_event;
            stream_by_event.stream_id = stream_->cloud_name();
            stream_by_event.pre_event = pre_rec_time_.count();
            stream_by_event.post_event = post_rec_time_.count();
            INJECT_CMD_WAIT_FOR_MATCHED_REPLY(
                transport_, stream_by_event,
                command_reply_done_OK(command::SET_STREAM_BY_EVENT));

            // get_cam_events -> cam_events_conf
            INJECT_CMD_WAIT_FOR_MATCHED_REPLY(
                transport_, get_cam_events(),
                command_reply(command::GET_CAM_EVENTS,
                              command::CAM_EVENTS_CONF));
        }
    }

    void START_STREAM(proto::stream_reason reason) {
        command::stream_start stream_start;

        stream_start.stream_id = supported_stream_.streams[0].id;
        stream_start.reason = reason;

        INJECT_CMD_WAIT_FOR_MATCHED_REPLY(
            transport_, stream_start,
            command_reply_done_OK(command::STREAM_START));
    }

    void STOP_STREAM(proto::stream_reason reason) {
        command::stream_stop stream_stop;

        stream_stop.stream_id = supported_stream_.streams[0].id;
        stream_stop.reason = reason;

        INJECT_CMD_WAIT_FOR_MATCHED_REPLY(
            transport_, stream_stop,
            command_reply_done_OK(command::STREAM_STOP));
    }

    virtual void TearDown() {
        realtime_timer_helper_.stop();
        manager_->stop();
        // Clears reference to manager_ as we used it to bind
        // manager::on_receive() for transport_
        transport_->set_handle_cb(nullptr);
    }
};