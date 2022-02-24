#include <agent-proto/proto.h>
#include <gtest/gtest.h>
#include <utils/utils.h>

using namespace ::testing;
using namespace std;
using namespace vxg::cloud;
using namespace vxg::cloud::agent;
using json = nlohmann::json;

#ifdef HAVE_CPP_LIB_FILESYSTEM
#include <experimental/filesystem>
#endif

TEST(utils, MotionMapUnpack) {
    std::string input_map =
        "0001111111111111100000000111111111111110000000011111111111111000001111"
        "1111111111111000001111111111111111100000111111111111111110000011111111"
        "1111111111111111111111111111111111111111111111111111111111111111111111"
        "1111111111111111111111111111111111111111111111111111111111111111111111"
        "1111110000111111111111111111000011111111111111111100001111111111111111"
        "1100001111111111111111000000111111111111111100";

    std::string packed_map = utils::motion::map::pack(input_map);
    vxg::logger::info("{}", packed_map);

    auto unpacked_map =
        utils::motion::map::unpack(packed_map, input_map.size());

    EXPECT_STREQ(unpacked_map.c_str(), input_map.c_str());
}

TEST(utils, EventObjectEnum) {
    json j_event_standard = {{"event", "motion"}};
    json j_event_custom = {{"event", "custom"},
                           {"custom_event_name", "custom1"}};
    json j_event_invalid = {{"event", "foobar"}};
    proto::event_object e1, e2, e3;

    e1 = j_event_standard;
    e2 = j_event_custom;
    e3 = j_event_invalid;

    EXPECT_EQ(e1.event, proto::ET_MOTION);
    EXPECT_EQ(e2.event, proto::ET_CUSTOM);
    EXPECT_EQ(e3.event, proto::ET_CUSTOM);
}

TEST(utils, UnsetJson) {
    json j_arr(json::value_t::array);
    j_arr.clear();
    j_arr.push_back({{"k1", "v1"}});
    j_arr.push_back({{"k2", "v2"}});
    json j_event1 = {{"event", "motion"}, {"meta", j_arr}};
    proto::event_object e1;
    json j_event2 = {{"event", "motion"}, {"meta", json::value_t::array}};
    proto::event_object e2;

    vxg::logger::info(j_event1.dump(2));
    vxg::logger::info(j_event2.dump(2));

    e1 = j_event1;
    e2 = j_event2;

    EXPECT_EQ(e1.event, proto::ET_MOTION);
    EXPECT_EQ(e2.event, proto::ET_MOTION);
    EXPECT_FALSE(__is_unset(e1.meta));
    EXPECT_TRUE(__is_unset(e2.meta));
}

TEST(utils, EventMeta) {
    json j_arr = {{"k1", "v1"}, {"k2", "v2"}};
    json j_event1 = {{"event", "motion"}, {"meta", j_arr}};
    proto::event_object e1;
    json j_event2 = {{"event", "motion"}, {"meta", json()}};
    proto::event_object e2;
    json j_event3 = {{"event", "motion"}};
    proto::event_object e3;
    json j_event4 = {{"event", "motion"}};
    proto::event_object e4;

    vxg::logger::info(j_event1.dump(2));
    vxg::logger::info(j_event2.dump(2));
    vxg::logger::info(j_event3.dump(2));
    vxg::logger::info(j_event4.dump(2));

    e1 = j_event1;
    e2 = j_event2;
    e3 = j_event3;
    e4 = j_event4;

    for (size_t i = 0; i < 3; i++) {
        e1.meta.update(
            {{"record" + std::to_string(i), "test" + std::to_string(i)}});
    }

    for (size_t i = 0; i < 3; i++) {
        e2.meta.update(
            {{"record" + std::to_string(i), "test" + std::to_string(i)}});
    }

    for (size_t i = 0; i < 3; i++) {
        e3.meta.update(
            {{"record" + std::to_string(i), "test" + std::to_string(i)}});
    }

    auto je1 = json(e1);
    auto je2 = json(e2);
    auto je3 = json(e3);
    auto je4 = json(e3);

    vxg::logger::info(je1.dump(2));
    vxg::logger::info(je2.dump(2));
    vxg::logger::info(je3.dump(2));
    vxg::logger::info(je4.dump(2));

    EXPECT_EQ(e1.event, proto::ET_MOTION);
    EXPECT_EQ(e2.event, proto::ET_MOTION);
    EXPECT_EQ(e3.event, proto::ET_MOTION);
    EXPECT_FALSE(__is_unset(e1.meta));
    EXPECT_TRUE(e1.meta.contains("k1"));
    EXPECT_TRUE(e1.meta.contains("k2"));
    EXPECT_TRUE(e1.meta.contains("record0"));
    EXPECT_TRUE(e1.meta.contains("record1"));
    EXPECT_TRUE(e1.meta.contains("record2"));
    EXPECT_TRUE(e2.meta.contains("record0"));
    EXPECT_TRUE(e2.meta.contains("record1"));
    EXPECT_TRUE(e2.meta.contains("record2"));
    EXPECT_TRUE(e3.meta.contains("record0"));
    EXPECT_TRUE(e3.meta.contains("record1"));
    EXPECT_TRUE(e3.meta.contains("record2"));
    EXPECT_FALSE(__is_unset(e2.meta));
    EXPECT_FALSE(__is_unset(e3.meta));
    EXPECT_TRUE(__is_unset(e4.meta));
}

#include <fstream>
inline bool __grep(const std::string& filepath, std::string grep_pattern) {
    bool result = false;
    try {
        std::regex pattern(grep_pattern, std::regex_constants::grep);
        std::ifstream fp(filepath);
        std::string line;
        std::smatch match;

        if (fp.is_open()) {
            while (getline(fp, line)) {
                if (regex_search(line, match, pattern)) {
                    result = true;
                    break;
                }
            }
            fp.close();
        }
    } catch (const std::regex_error& e) {
        vxg::logger::error("grep error: {}", e.what());
    }
    return result;
}
constexpr char* TEST_LOGFILE = "/tmp/test.log";
constexpr char* TEST_LOGFILE_CRASH = "/tmp/test_crash.log";
TEST(utils, LoggingFileReset) {
    int argc = 1;
    char* argv[] = {"foo", nullptr};
    vxg::logger::logger_ptr logger = vxg::logger::instance("mytestlogger");

    try {
        std::experimental::filesystem::remove(TEST_LOGFILE);
        std::experimental::filesystem::remove(TEST_LOGFILE_CRASH);
    } catch (...) {
    }

    logger->info("LOG0");
    vxg::logger::reset(argc, argv, vxg::logger::lvl_trace, "",
                       TEST_LOGFILE_CRASH, TEST_LOGFILE);
    logger->info("LOG1");
    vxg::logger::reset(argc, argv, vxg::logger::lvl_trace, "IDENT1");
    logger->info("LOG2");
    vxg::logger::reset(argc, argv, vxg::logger::lvl_trace, "",
                       TEST_LOGFILE_CRASH, TEST_LOGFILE);
    logger->info("LOG3");
    vxg::logger::reset(argc, argv, vxg::logger::lvl_trace, "IDENT2");
    logger->info("LOG4");

    EXPECT_FALSE(__grep(TEST_LOGFILE, "LOG0"));
    EXPECT_TRUE(__grep(TEST_LOGFILE, "LOG1"));
    EXPECT_FALSE(__grep(TEST_LOGFILE, "LOG2"));
    EXPECT_TRUE(__grep(TEST_LOGFILE, "LOG3"));
    EXPECT_FALSE(__grep(TEST_LOGFILE, "LOG4"));
}

TEST(utils, time_to_from_iso) {
    std::string str_iso_begin = "2021-11-17T10:44:00.409714Z";
    std::string str_iso_end = "2021-11-17T10:44:09.709714Z";
    auto from_iso_begin = utils::time::from_iso(str_iso_begin);
    auto from_iso_end = utils::time::from_iso(str_iso_end);
    auto to_iso_packed_begin = utils::time::to_iso_packed(from_iso_begin);
    auto to_iso_packed_end = utils::time::to_iso_packed(from_iso_end);
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           from_iso_end - from_iso_begin)
                           .count();
    auto begin = utils::time::from_iso_packed(to_iso_packed_begin);
    auto end = begin + std::chrono::milliseconds(duration_ms);
    auto begin_iso = utils::time::to_iso(begin);
    auto end_iso = utils::time::to_iso(end);

    EXPECT_EQ(end, begin + std::chrono::milliseconds(duration_ms));
    EXPECT_STREQ(begin_iso.c_str(), str_iso_begin.c_str());
    EXPECT_STREQ(end_iso.c_str(), str_iso_end.c_str());
    EXPECT_EQ(begin, from_iso_begin);
    EXPECT_EQ(end, from_iso_end);
}