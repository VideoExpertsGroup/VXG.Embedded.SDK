#include <gtest/gtest.h>
#include <utils/properties.h>

using namespace ::testing;
using namespace std;
using namespace vxg;

class TestProperties : public Test {
protected:
    TestProperties() {}

    virtual void SetUp() {
        random_file_ = "/tmp/test-log-" + std::to_string(rand());
        properties::reset(random_file_);
    }

    virtual void TearDown() { unlink(random_file_.c_str()); }

public:
    std::string random_file_;
};

TEST_F(TestProperties, DefaultConstructor) {
    for (int i = 0; i < 10; i++) {
        // Arrange
        int test_cookie = rand() % 10000;
        std::string test_key = "Test" + std::to_string(test_cookie);
        std::string test_val = "TestValue" + std::to_string(test_cookie);

        // Act
        properties::set(test_key, test_val);

        // Assert
        EXPECT_STREQ(properties::get(test_key).c_str(), test_val.c_str());
    }
}

TEST_F(TestProperties, SetEmpty) {
    std::string testEmpty;
    std::string testNonEmpty;

    // No file, should be empty
    testEmpty = properties::get("TEST_FIELD");
    // File should be created and key saved
    properties::set("TEST_FIELD", "TEST");
    testNonEmpty = properties::get("TEST_FIELD");

    // Assert
    EXPECT_TRUE(testEmpty.empty());
    EXPECT_FALSE(testNonEmpty.empty());
    EXPECT_STREQ(testNonEmpty.c_str(), "TEST");
}