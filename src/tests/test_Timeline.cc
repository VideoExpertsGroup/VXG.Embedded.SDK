#include <gtest/gtest.h>

#include <agent/timeline.h>

using namespace ::testing;
using namespace std;
using namespace vxg::cloud;
using namespace vxg::cloud::agent;
using json = nlohmann::json;

const std::string token =
    "eyJjYW1pZCI6IDIyNzA4NCwgImNtbmdyaWQiOiAyMjc1MDgsICJhY2Nlc3MiOiAiYWxsIiwgIn"
    "Rva2VuIjogInNoYXJlLmV5SnphU0k2SURFNE5UUTROMzAuNWZiNjcwNzl0YmMxZmFiMDAuYWE4"
    "XzViTlBvSjktdWdtZFdRRjk2S0E5WWVRIiwgImFwaSI6ICJ3ZWIuc2t5dnIudmlkZW9leHBlcn"
    "RzZ3JvdXAuY29tIiwgImNhbSI6ICJjYW0uc2t5dnIudmlkZW9leHBlcnRzZ3JvdXAuY29tIn0"
    "=";

class TestTimeline : public Test {
protected:
    TestTimeline() {}

    virtual void SetUp() {
        vfs1 = std::make_shared<vfs_storage>("/tmp/vfs1/");
        vfs2 = std::make_shared<vfs_storage>("/tmp/vfs2/");
        cloud1 =
            std::make_shared<cloud_storage>(proto::access_token::parse(token));
        timeline1 = std::make_shared<timeline<vfs_storage>>("/tmp/vfs1/");
        timeline2 = std::make_shared<timeline<cloud_storage>>(
            proto::access_token::parse(token));
    }

    virtual void TearDown() {}

public:
    std::shared_ptr<vfs_storage> vfs1;
    std::shared_ptr<vfs_storage> vfs2;
    std::shared_ptr<cloud_storage> cloud1;
    std::shared_ptr<timeline<vfs_storage>> timeline1;
    std::shared_ptr<timeline<cloud_storage>> timeline2;
};

TEST_F(TestTimeline, Timeline_StoreListLoadVFS) {
    using namespace utils::time;

    auto now = std::chrono::system_clock::now();
    std::vector<uint8_t> data(10);
    period p1 {now, now + std::chrono::seconds(1)};
    timed_storage::item_ptr item =
        std::make_shared<timed_storage::item>(p1, data);

    auto ret_store1 = vfs1->store(item);
    auto ret_list1 = vfs1->list(p1.begin, p1.end);
    auto ret_store2 =
        vfs1->load(std::make_shared<timed_storage::item>(p1.begin, p1.end));

    for (auto& f : ret_list1)
        vxg::logger::info("{} - {}", to_iso(f->begin), to_iso(f->end));

    EXPECT_TRUE(ret_store1);
    EXPECT_TRUE(ret_store2);
    EXPECT_EQ(ret_list1.size(), 1);
}

TEST_F(TestTimeline, Timeline_Store3XSlices) {
    using namespace utils::time;

    vxg::cloud::time now = vxg::cloud::utils::time::now();

    std::vector<uint8_t> data(10);
    period p1 {now - std::chrono::seconds(1), now};
    period p2 {now - std::chrono::seconds(2), now - std::chrono::seconds(1)};
    period p3 {now - std::chrono::seconds(3), now - std::chrono::seconds(2)};
    timed_storage::item_ptr item1 =
        std::make_shared<timed_storage::item>(p1, data);
    timed_storage::item_ptr item2 =
        std::make_shared<timed_storage::item>(p2, data);
    timed_storage::item_ptr item3 =
        std::make_shared<timed_storage::item>(p3, data);

    auto ret_vfs_store1 = vfs1->store(item1);
    auto ret_vfs_store2 = vfs1->store(item2);
    auto ret_vfs_store3 = vfs1->store(item3);

    auto ret_timeline_slices1 =
        timeline1->slices(now - std::chrono::seconds(3), now);
    auto ret_list1 = vfs1->list(now - std::chrono::seconds(3), now);

    EXPECT_TRUE(ret_vfs_store1);
    EXPECT_TRUE(ret_vfs_store2);
    EXPECT_TRUE(ret_vfs_store3);
    EXPECT_EQ(ret_timeline_slices1.size(), 1);
    EXPECT_EQ(ret_timeline_slices1[0].begin, now - std::chrono::seconds(3));
    EXPECT_EQ(ret_timeline_slices1[0].end, now);
    EXPECT_EQ(ret_list1.size(), 3);
}

TEST_F(TestTimeline, Timeline_ListCloud) {
    using namespace utils::time;
    vxg::cloud::time now = vxg::cloud::utils::time::now();

    auto ret_list1 = cloud1->list(now - std::chrono::hours(1), now);

    for (auto& r : ret_list1) {
        vxg::logger::info("{} - {}", to_iso(r->begin), to_iso(r->end));
    }

    EXPECT_GE(ret_list1.size(), 1);
}

TEST_F(TestTimeline, Timeline_LoadCloudStoreVFS) {
    using namespace utils::time;
    vxg::cloud::time now = vxg::cloud::utils::time::now();
    auto i =
        std::make_shared<timed_storage::item>(now - std::chrono::hours(1), now);

    auto ret_load1 = cloud1->load(i);
    vxg::logger::info("Storing {} bytes for {} - {}", i->data.size(),
                      to_iso(i->begin), to_iso(i->end));
    auto ret_store1 = vfs1->store(i);

    EXPECT_TRUE(ret_load1);
    EXPECT_TRUE(ret_store1);
}

TEST_F(TestTimeline, Timeline_LoadVFSStoreCloud) {
    using namespace utils::time;
    vxg::cloud::time now = vxg::cloud::utils::time::now();

    auto i = vfs1->list(now - std::chrono::hours(24), now);
    auto ret_load1 = !i.empty() && vfs1->load(i.front());
    auto ret_store1 = ret_load1 && cloud1->store(i.front());

    EXPECT_TRUE(ret_load1);
    EXPECT_TRUE(ret_store1);
}