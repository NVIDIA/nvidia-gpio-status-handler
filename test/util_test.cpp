#include "nlohmann/json.hpp"
#include "util.hpp"

#include <gtest/gtest.h>

using namespace util;

TEST(Util, GetDeviceId)
{
    EXPECT_EQ(getDeviceId("GPU5"), 5);
    EXPECT_EQ(getDeviceId("GPU6-ERoT"), 6);
    EXPECT_EQ(getDeviceId("GPU0"), 0);
    EXPECT_EQ(getDeviceId("PCIeSwitch"), 0); // using defaults
    EXPECT_EQ(getDeviceId(""), 0);           // using defaults
    EXPECT_EQ(getDeviceId("Retimer3"), 3);
    EXPECT_EQ(getDeviceId("NVSwitch1-ERoT"), 1);
    EXPECT_EQ(getDeviceId("GPU6-ERoT", "GPU[0-7]-ERoT"), 6);
    EXPECT_EQ(getDeviceId("GPU9", "GPU[0-7]"), -1);
}

TEST(UtilExpandDeviceRange, OnlyRange)
{
    auto devMap = expandDeviceRange("[0-1]");
    EXPECT_EQ(devMap.size(), 2);
    EXPECT_EQ(devMap.count(0), 1);
    EXPECT_EQ(devMap.count(1), 1);
    EXPECT_EQ(devMap.at(0), "0");
    EXPECT_EQ(devMap.at(1), "1");
}

TEST(UtilExpandDeviceRange, RangeAtEnd)
{
    auto devMap = expandDeviceRange("begin[0-1]");
    EXPECT_EQ(devMap.size(), 2);
    EXPECT_EQ(devMap.count(0), 1);
    EXPECT_EQ(devMap.count(1), 1);
    EXPECT_EQ(devMap.at(0), "begin0");
    EXPECT_EQ(devMap.at(1), "begin1");
}

TEST(UtilExpandDeviceRange, RangeAtBeginning)
{
    auto devMap = expandDeviceRange("[0-1]end");
    EXPECT_EQ(devMap.size(), 2);
    EXPECT_EQ(devMap.count(0), 1);
    EXPECT_EQ(devMap.count(1), 1);
    EXPECT_EQ(devMap.at(0), "0end");
    EXPECT_EQ(devMap.at(1), "1end");
}

TEST(UtilExpandDeviceRange, RangeMiddle)
{
    auto devMap = expandDeviceRange("begin[0-1]end");
    EXPECT_EQ(devMap.size(), 2);
    EXPECT_EQ(devMap.count(0), 1);
    EXPECT_EQ(devMap.count(1), 1);
    EXPECT_EQ(devMap.at(0), "begin0end");
    EXPECT_EQ(devMap.at(1), "begin1end");
}

TEST(UtilExpandDeviceRange, NoRange)
{
    auto devMap = expandDeviceRange("test");
    EXPECT_EQ(devMap.size(), 1);
    EXPECT_EQ(devMap.count(0), 1);
    EXPECT_EQ(devMap.at(0), "test");
}

TEST(UtilExpandDeviceRange, NoRangeButDeviceId)
{
    auto devMap = expandDeviceRange("GPU5");
    EXPECT_EQ(devMap.size(), 1);
    EXPECT_EQ(devMap.count(5), 1);
    EXPECT_EQ(devMap.at(5), "GPU5");
}

TEST(UtilExpandDeviceRange, Empty)
{
    auto devMap = expandDeviceRange("");
    EXPECT_EQ(devMap.size(), 0);
}

TEST(UtilreplaceRangeByMatchedValue, IsolatedRangeMathedAtEnd)
{
    auto ret = replaceRangeByMatchedValue("begin [0-7]", "GPU6");
    EXPECT_EQ(ret, "begin GPU6");

    ret = replaceRangeByMatchedValue("begin [5-7]", "GPU6");
    EXPECT_EQ(ret, "begin GPU6");

    ret = replaceRangeByMatchedValue("begin [6-7]", "GPU6");
    EXPECT_EQ(ret, "begin GPU6");

    // does not match
    ret = replaceRangeByMatchedValue("begin [0-7]", "none");
    EXPECT_EQ(ret, "begin [0-7]");

    // out of range
    ret = replaceRangeByMatchedValue("begin [0-7]", "GPU8");
    EXPECT_EQ(ret, "begin [0-7]");

    // out of range
    ret = replaceRangeByMatchedValue("begin [2-7]", "GPU0");
    EXPECT_EQ(ret, "begin [2-7]");
    ret = replaceRangeByMatchedValue("begin [2-7]", "GPU1");
    EXPECT_EQ(ret, "begin [2-7]");
}
