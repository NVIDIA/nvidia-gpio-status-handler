#include "device_id.hpp"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/token_iterator.hpp>
#include <boost/tokenizer.hpp>

#include <array>
#include <charconv>
#include <iostream>
#include <list>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace testing;
using namespace std::literals;

namespace device_id
{

TEST(DeviceIdTest, PatternIndex_Construction)
{
    std::cout << PatternIndex() << std::endl;
    std::cout << PatternIndex(7) << std::endl;
    std::cout << PatternIndex(4, 6, 5) << std::endl;
    std::cout << PatternIndex(-3, 7) << std::endl;
    std::cout << PatternIndex(-3, 7, -2, -1) << std::endl;
}

TEST(DeviceIdTest, PatternIndex_dim)
{
    EXPECT_EQ(PatternIndex().dim(), 0);
    EXPECT_EQ(PatternIndex(7).dim(), 1);
    EXPECT_EQ(PatternIndex(4, 6, 5).dim(), 3);
    EXPECT_EQ(PatternIndex(-3, 7).dim(), 2);
    EXPECT_EQ(PatternIndex(-3, 7, -2, -1).dim(), 2);
}

TEST(DeviceIdTest, PatternIndex_index)
{
    EXPECT_EQ(PatternIndex(7)[0], 7);
    EXPECT_EQ(PatternIndex(4, 6, 5)[0], 4);
    EXPECT_EQ(PatternIndex(4, 6, 5)[1], 6);
    EXPECT_EQ(PatternIndex(4, 6, 5)[2], 5);
    EXPECT_EQ(PatternIndex(4, 6, 5)[3], PatternIndex::unspecified);
    EXPECT_EQ(PatternIndex(4, 6, 5)[4], PatternIndex::unspecified);
}

TEST(DeviceIdTest, PatternInputMapping_contains_empty)
{
    PatternInputMapping emptyMapping;
    EXPECT_TRUE(emptyMapping.contains(PatternIndex::unspecified));
    EXPECT_TRUE(emptyMapping.contains(1));
    EXPECT_TRUE(emptyMapping.contains(10));
    EXPECT_TRUE(emptyMapping.contains(0));
}

TEST(DeviceIdTest, PatternInputMapping_contains_regular)
{
    PatternInputMapping map(std::map<unsigned, unsigned>(
        {{1, 0}, {2, 1}, {4, 2}, {8, 3}, {16, 4}}));
    EXPECT_TRUE(map.contains(1));
    EXPECT_TRUE(map.contains(8));
    EXPECT_FALSE(map.contains(9));
    EXPECT_FALSE(map.contains(PatternIndex::unspecified));
}

TEST(DeviceIdTest, PatternInputMapping_eval_specified)
{
    PatternInputMapping map(std::map<unsigned, unsigned>(
        {{1, 0}, {2, 1}, {4, 2}, {8, 3}, {16, 4}}));
    EXPECT_EQ(map.eval(16), 4);
    EXPECT_EQ(map.eval(4), 2);
    EXPECT_ANY_THROW(map.eval(5));
    EXPECT_ANY_THROW(map.eval(PatternIndex::unspecified));
}

TEST(DeviceIdTest, PatternInputMapping_eval_unspecified)
{
    PatternInputMapping map;
    EXPECT_ANY_THROW(map.eval(16));
    EXPECT_ANY_THROW(map.eval(4));
    EXPECT_ANY_THROW(map.eval(5));
    EXPECT_ANY_THROW(map.eval(PatternIndex::unspecified));
}

template <class CharT>
std::basic_ostream<CharT>& operator<<(std::basic_ostream<CharT>& os,
                                      const PatternInputMapping& pim)
{
    if (pim.unspecified)
    {
        os << "PatternInputMapping([unspecified])";
    }
    else // ! pim.unspecified
    {
        os << "PatternInputMapping(";
        testing::internal::PrintTo(pim.valuesMapping, &os);
        os << ")";
    }
    return os;
}

TEST(DeviceIdTest, CartesianProductRange_PatternInputMapping)
{
    for (const auto& elem : CartesianProductRange(
             {PatternInputMapping({{4, 40}, {5, 50}, {8, 80}}),
              PatternInputMapping({{7, 70}, {5, 50}, {9, 90}, {10, 100}}),
              PatternInputMapping({{100, 1000}, {200, 2000}})}))
    {
        std::cout << "elem = '" << elem << "'" << std::endl;
    }
}

TEST(DeviceIdTest, CartesianProductRange_PatternInputMapping1)
{
    for (const auto& elem : CartesianProductRange(
             {PatternInputMapping({{4, 40}, {5, 50}, {8, 80}}),
              PatternInputMapping(),
              PatternInputMapping({{100, 1000}, {200, 2000}})}))
    {
        std::cout << "elem = '" << elem << "'" << std::endl;
    }
}

TEST(DeviceIdTest, DeviceIdPattern_calcInputIndexToBracketPoss)
{
    EXPECT_THAT(DeviceIdPattern::calcInputIndexToBracketPoss(
                    std::vector<unsigned>({0, 4, 0, 1, 1})),
                UnorderedElementsAre(
                    std::vector<unsigned>{0, 2}, std::vector<unsigned>{3, 4},
                    std::vector<unsigned>{}, std::vector<unsigned>{},
                    std::vector<unsigned>{1}));
}

TEST(DeviceIdTest, DeviceIdPattern_calcInputIndexToBracketPoss_empty)
{
    EXPECT_THAT(
        DeviceIdPattern::calcInputIndexToBracketPoss(std::vector<unsigned>()),
        UnorderedElementsAre());
}

TEST(DeviceIdTest, DeviceIdPattern_calcInputMapping)
{
    std::map<unsigned, unsigned> brackMap({{1, 100}, {2, 200}, {3, 300}});
    EXPECT_EQ(
        DeviceIdPattern::calcInputMapping(0, {0, 1}, {brackMap, brackMap}),
        PatternInputMapping(brackMap));
}

TEST(DeviceIdTest, DeviceIdPattern_calcInputMapping1)
{
    EXPECT_EQ(
        DeviceIdPattern::calcInputMapping(
            0, {0, 1}, {{{1, 100}, {2, 200}, {3, 300}}, {{1, 100}, {2, 200}}}),
        PatternInputMapping({{1, 100}, {2, 200}}));
}

TEST(DeviceIdTest, DeviceIdPattern_calcInputMapping2)
{
    EXPECT_EQ(DeviceIdPattern::calcInputMapping(0, {0, 1},
                                                {{}, {{1, 100}, {2, 200}}}),
              PatternInputMapping(std::map<unsigned, unsigned>{}));
}

TEST(DeviceIdTest, DeviceIdPattern_calcInputMapping3)
{
    EXPECT_ANY_THROW(DeviceIdPattern::calcInputMapping(
        0, {0, 1}, {{{1, 100}, {2, 300}}, {{1, 100}, {2, 200}}}));
}

TEST(DeviceIdTest, DeviceIdPattern_calcInputMappings)
{
    EXPECT_EQ(DeviceIdPattern::calcInputMappings(
                  {{0, 2}, {1}}, {{{1, 100}, {2, 200}, {3, 300}},
                                  {{5, 7}, {3, 11}, {1, 13}},
                                  {{1, 100}, {2, 200}, {3, 300}}}),
              std::vector<PatternInputMapping>(
                  {PatternInputMapping({{1, 100}, {2, 200}, {3, 300}}),
                   PatternInputMapping({{5, 7}, {3, 11}, {1, 13}})}));
}

TEST(DeviceIdTest, DeviceIdPattern_calcInputMappings1)
{
    EXPECT_EQ(DeviceIdPattern::calcInputMappings(
                  {{0, 2}, {}, {1}}, {{{1, 100}, {2, 200}, {3, 300}},
                                      {{5, 7}, {3, 11}, {1, 13}},
                                      {{1, 100}, {2, 200}, {3, 300}}}),
              std::vector<PatternInputMapping>(
                  {PatternInputMapping({{1, 100}, {2, 200}, {3, 300}}),
                   PatternInputMapping(),
                   PatternInputMapping({{5, 7}, {3, 11}, {1, 13}})}));
}

TEST(DeviceIdTest, DeviceIdPattern_calcInputMappings2)
{
    EXPECT_EQ(DeviceIdPattern::calcInputMappings({{}, {}, {0}},
                                                 {{{5, 7}, {3, 11}, {1, 13}}}),
              std::vector<PatternInputMapping>(
                  {PatternInputMapping(), PatternInputMapping(),
                   PatternInputMapping({{5, 7}, {3, 11}, {1, 13}})}));
}

TEST(DeviceIdTest, DeviceIdPattern_calcInputMappings3)
{
    EXPECT_EQ(DeviceIdPattern::calcInputMappings({}, {}),
              std::vector<PatternInputMapping>());
}

std::map<unsigned, unsigned>
    mappingsSum(const std::vector<std::map<unsigned, unsigned>>& maps);

// testing::internal::PrintTo(
//     mappingsSum(
//         {{{2, 4}, {4, 16}, {6, 36}}, {{1, 1}, {3, 9}, {5, 25}, {7, 49}}}),
//     &std::cout);

TEST(DeviceIdTest, mappingsSum_empty)
{
    std::map<unsigned, unsigned> val({});
    EXPECT_EQ(mappingsSum({}), val);
}

TEST(DeviceIdTest, mappingsSum_single)
{
    std::map<unsigned, unsigned> map(
        {{1, 1}, {2, 1}, {3, 2}, {4, 3}, {5, 5}, {6, 8}, {7, 13}});
    EXPECT_EQ(mappingsSum(std::vector<std::map<unsigned, unsigned>>{map}), map);
}

TEST(DeviceIdTest, mappingsSum_disjoint)
{
    std::map<unsigned, unsigned> result(
        {{1, 1}, {2, 4}, {3, 9}, {4, 16}, {5, 25}, {6, 36}, {7, 49}});
    EXPECT_EQ(mappingsSum({{{2, 4}, {4, 16}, {6, 36}},
                           {{1, 1}, {3, 9}, {5, 25}, {7, 49}}}),
              result);
}

TEST(DeviceIdTest, mappingsSum_overlapping)
{
    std::map<unsigned, unsigned> result(
        {{1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}, {6, 7}});
    EXPECT_EQ(mappingsSum({{{1, 2}, {2, 3}, {3, 4}, {4, 5}},
                           {{3, 4}, {4, 5}, {5, 6}, {6, 7}}}),
              result);
}

TEST(DeviceIdTest, DeviceIdPattern_domain_iteration_single)
{
    DeviceIdPattern pat("[0|1-4:2-5]");
    // DeviceIdPattern pat(std::vector<unsigned>({0}),
    //                     std::vector<std::map<unsigned, unsigned>>(
    //                         {{{1, 2}, {2, 3}, {3, 4}, {4, 5}}}));
    unsigned i = 0;
    PatternIndex prevPi;
    for (const auto& pi : pat.domain())
    {
        std::cout << "pattern index = '" << pi << "'" << std::endl;
        switch (i)
        {
            case 0:
                EXPECT_EQ(pi, PatternIndex(1));
                break;
            case 1:
                EXPECT_EQ(pi, PatternIndex(2));
                break;
            case 2:
                EXPECT_EQ(pi, PatternIndex(3));
                break;
            case 3:
                EXPECT_EQ(pi, PatternIndex(4));
                break;
        }
        EXPECT_TRUE(prevPi < pi);
        prevPi = pi;
        ++i;
    }
    EXPECT_EQ(i, 4);
}

TEST(DeviceIdTest, DeviceIdPattern_domain_iteration_regular)
{
    DeviceIdPattern pat("[0|1-4:2-5][1|3-6:4-7][0|1-4:2-5]");
    // DeviceIdPattern pat(std::vector<unsigned>({0, 1, 0}),
    //                     std::vector<std::map<unsigned, unsigned>>(
    //                         {{{1, 2}, {2, 3}, {3, 4}, {4, 5}},
    //                          {{3, 4}, {4, 5}, {5, 6}, {6, 7}},
    //                          {{1, 2}, {2, 3}, {3, 4}, {4, 5}}}));
    unsigned i = 0;
    PatternIndex prevPi;
    for (const auto& pi : pat.domain())
    {
        std::cout << "pattern index = '" << pi << "'" << std::endl;
        switch (i)
        {
            case 0:
                EXPECT_EQ(pi, PatternIndex(1, 3));
                break;
            case 1:
                EXPECT_EQ(pi, PatternIndex(1, 4));
                break;
            case 2:
                EXPECT_EQ(pi, PatternIndex(1, 5));
                break;
            case 3:
                EXPECT_EQ(pi, PatternIndex(1, 6));
                break;
            case 4:
                EXPECT_EQ(pi, PatternIndex(2, 3));
                break;
            case 5:
                EXPECT_EQ(pi, PatternIndex(2, 4));
                break;
            case 6:
                EXPECT_EQ(pi, PatternIndex(2, 5));
                break;
            case 7:
                EXPECT_EQ(pi, PatternIndex(2, 6));
                break;
            case 8:
                EXPECT_EQ(pi, PatternIndex(3, 3));
                break;
            case 9:
                EXPECT_EQ(pi, PatternIndex(3, 4));
                break;
            case 10:
                EXPECT_EQ(pi, PatternIndex(3, 5));
                break;
            case 11:
                EXPECT_EQ(pi, PatternIndex(3, 6));
                break;
            case 12:
                EXPECT_EQ(pi, PatternIndex(4, 3));
                break;
            case 13:
                EXPECT_EQ(pi, PatternIndex(4, 4));
                break;
            case 14:
                EXPECT_EQ(pi, PatternIndex(4, 5));
                break;
            case 15:
                EXPECT_EQ(pi, PatternIndex(4, 6));
                break;
        }
        EXPECT_TRUE(prevPi < pi);
        prevPi = pi;
        ++i;
    }
    EXPECT_EQ(i, 16);
}

TEST(DeviceIdTest, DeviceIdPattern_domain_iteration_gap)
{
    DeviceIdPattern pat("[3|1-4:2-5]");
    // DeviceIdPattern pat(std::vector<unsigned>({3}),
    //                     std::vector<std::map<unsigned, unsigned>>(
    //                         {{{1, 2}, {2, 3}, {3, 4}, {4, 5}}}));
    unsigned i = 0;
    PatternIndex prevPi;
    for (const auto& pi : pat.domain())
    {
        std::cout << "pattern index = '" << pi << "'" << std::endl;
        switch (i)
        {
            case 0:
                EXPECT_EQ(pi, PatternIndex(PatternIndex::unspecified,
                                           PatternIndex::unspecified,
                                           PatternIndex::unspecified, 1));
                break;
            case 1:
                EXPECT_EQ(pi, PatternIndex(PatternIndex::unspecified,
                                           PatternIndex::unspecified,
                                           PatternIndex::unspecified, 2));
                break;
            case 2:
                EXPECT_EQ(pi, PatternIndex(PatternIndex::unspecified,
                                           PatternIndex::unspecified,
                                           PatternIndex::unspecified, 3));
                break;
            case 3:
                EXPECT_EQ(pi, PatternIndex(PatternIndex::unspecified,
                                           PatternIndex::unspecified,
                                           PatternIndex::unspecified, 4));
                break;
        }
        EXPECT_TRUE(prevPi < pi);
        prevPi = pi;
        ++i;
    }
    EXPECT_EQ(i, 4);
}

TEST(DeviceIdTest, DeviceIdPattern_domain_iteration_gap1)
{
    DeviceIdPattern pat("[1|21-22:11][3|1-4:2-5]");
    // DeviceIdPattern pat(
    //     std::vector<unsigned>({1, 3}),
    //     std::vector<std::map<unsigned, unsigned>>(
    //         {{{21, 11}, {22, 11}}, {{1, 2}, {2, 3}, {3, 4}, {4, 5}}}));
    unsigned i = 0;
    PatternIndex prevPi;
    for (const auto& pi : pat.domain())
    {
        std::cout << "pattern index = '" << pi << "'" << std::endl;
        switch (i)
        {
            case 0:
                EXPECT_EQ(pi, PatternIndex(PatternIndex::unspecified, 21,
                                           PatternIndex::unspecified, 1));
                break;
            case 1:
                EXPECT_EQ(pi, PatternIndex(PatternIndex::unspecified, 21,
                                           PatternIndex::unspecified, 2));
                break;
            case 2:
                EXPECT_EQ(pi, PatternIndex(PatternIndex::unspecified, 21,
                                           PatternIndex::unspecified, 3));
                break;
            case 3:
                EXPECT_EQ(pi, PatternIndex(PatternIndex::unspecified, 21,
                                           PatternIndex::unspecified, 4));
                break;
            case 4:
                EXPECT_EQ(pi, PatternIndex(PatternIndex::unspecified, 22,
                                           PatternIndex::unspecified, 1));
                break;
            case 5:
                EXPECT_EQ(pi, PatternIndex(PatternIndex::unspecified, 22,
                                           PatternIndex::unspecified, 2));
                break;
            case 6:
                EXPECT_EQ(pi, PatternIndex(PatternIndex::unspecified, 22,
                                           PatternIndex::unspecified, 3));
                break;
            case 7:
                EXPECT_EQ(pi, PatternIndex(PatternIndex::unspecified, 22,
                                           PatternIndex::unspecified, 4));
                break;
        }
        EXPECT_TRUE(prevPi < pi);
        prevPi = pi;
        ++i;
    }
    EXPECT_EQ(i, 8);
}

TEST(DeviceIdTest, DeviceIdPattern_domain_iteration_empty)
{
    DeviceIdPattern pat;
    // DeviceIdPattern pat = DeviceIdPattern(
    //     std::vector<unsigned>(), std::vector<std::map<unsigned,
    //     unsigned>>());
    std::cout << "before loop" << std::endl;
    unsigned i = 0;
    for (const auto& pi : pat.domain())
    {
        std::cout << "pattern index = '" << pi << "'" << std::endl;
        switch (i)
        {
            case 0:
                EXPECT_EQ(pi, PatternIndex());
                break;
        }
        ++i;
    }
    EXPECT_EQ(i, 1);
}

// TEST(DeviceIdTest, DeviceIdPattern_domainVec)
// {
//     DeviceIdPattern pat =
//         DeviceIdPattern(std::vector<unsigned>({1, 0}),
//                         std::vector<std::map<unsigned, unsigned>>(
//                             {{{17, 71}, {13, 31}}, {{731, 137}, {246,
//                             642}}}));
//     EXPECT_THAT(pat.domainVec(),
//                 ElementsAre(PatternIndex(246, 13), PatternIndex(246, 17),
//                             PatternIndex(731, 13), PatternIndex(731, 17)));
// }

// TEST(DeviceIdTest, DeviceIdPattern_checkIsInDomain_regular)
// {
//     DeviceIdPattern pat =
//         DeviceIdPattern(std::vector<unsigned>({1, 0}),
//                         std::vector<std::map<unsigned, unsigned>>(
//                             {{{17, 71}, {13, 31}}, {{731, 137}, {246,
//                             642}}}));
//     EXPECT_ANY_THROW(
//         pat.checkIsInDomain(PatternIndex(PatternIndex::unspecified, 17)));
//     EXPECT_NO_THROW(pat.checkIsInDomain(PatternIndex(731, 13)));
//     EXPECT_NO_THROW(pat.checkIsInDomain(PatternIndex(731, 13, 5)));
//     EXPECT_ANY_THROW(pat.checkIsInDomain(PatternIndex(731)));
//     EXPECT_ANY_THROW(pat.checkIsInDomain(PatternIndex()));
// }

// TEST(DeviceIdTest, DeviceIdPattern_checkIsInDomain_gap)
// {
//     DeviceIdPattern pat = DeviceIdPattern(
//         std::vector<unsigned>({1}),
//         std::vector<std::map<unsigned, unsigned>>({{{17, 71}, {13, 31}}}));
//     EXPECT_NO_THROW(
//         pat.checkIsInDomain(PatternIndex(PatternIndex::unspecified, 17)));
//     EXPECT_NO_THROW(pat.checkIsInDomain(PatternIndex(31337, 17)));
//     EXPECT_ANY_THROW(
//         pat.checkIsInDomain(PatternIndex(17, PatternIndex::unspecified)));
//     EXPECT_ANY_THROW(pat.checkIsInDomain(PatternIndex()));
//     EXPECT_ANY_THROW(pat.checkIsInDomain(PatternIndex(17)));
// }

TEST(DeviceIdTest, DeviceIdPattern_checkIsInDomain_empty)
{
    DeviceIdPattern pat;
    // DeviceIdPattern pat = DeviceIdPattern(
    //     std::vector<unsigned>(), std::vector<std::map<unsigned,
    //     unsigned>>());
    EXPECT_NO_THROW(pat.checkIsInDomain(PatternIndex()));
    EXPECT_NO_THROW(
        pat.checkIsInDomain(PatternIndex(PatternIndex::unspecified)));
    EXPECT_NO_THROW(pat.checkIsInDomain(PatternIndex(1, 4)));
}

TEST(DeviceIdTest, DeviceIdPattern_eval)
{
    DeviceIdPattern pat("abc_[1|17-18:71-72]_def_[0|731-732:246-247]_ghi");
    EXPECT_EQ(pat.eval(PatternIndex(731, 18)), "abc_72_def_246_ghi");
    EXPECT_EQ(pat.eval(PatternIndex(732, 18)), "abc_72_def_247_ghi");
    EXPECT_EQ(pat.eval(PatternIndex(731, 17)), "abc_71_def_246_ghi");
    EXPECT_EQ(pat.eval(PatternIndex(732, 17)), "abc_71_def_247_ghi");
    EXPECT_EQ(pat.eval(PatternIndex(732, 17, 100)), "abc_71_def_247_ghi");
    EXPECT_EQ(pat.eval(PatternIndex(732, 17, 100, 9)), "abc_71_def_247_ghi");
    EXPECT_ANY_THROW(pat.eval(PatternIndex(732)));
    EXPECT_ANY_THROW(pat.eval(PatternIndex(732, 19)));
}

// TEST(DeviceIdTest, DeviceIdPattern_eval)
// {
//     DeviceIdPattern pat = DeviceIdPattern(
//         {"abc_"sv, "_def_"sv, "_ghi"sv}, std::vector<unsigned>({1, 0}),
//         std::vector<std::map<unsigned, unsigned>>(
//             {{{17, 71}, {13, 31}}, {{731, 137}, {246, 642}}}));

//     EXPECT_EQ(pat.eval(PatternIndex(731, 13)), "abc_31_def_137_ghi");
//     EXPECT_EQ(pat.eval(PatternIndex(246, 13)), "abc_31_def_642_ghi");
//     EXPECT_EQ(pat.eval(PatternIndex(731, 17)), "abc_71_def_137_ghi");
//     EXPECT_EQ(pat.eval(PatternIndex(246, 17)), "abc_71_def_642_ghi");
//     EXPECT_EQ(pat.eval(PatternIndex(246, 17, 100)), "abc_71_def_642_ghi");
//     EXPECT_EQ(pat.eval(PatternIndex(246, 17, 100, 9)), "abc_71_def_642_ghi");
//     EXPECT_ANY_THROW(pat.eval(PatternIndex(246)));
//     EXPECT_ANY_THROW(pat.eval(PatternIndex(246, 18)));
// }

TEST(DeviceIdTest, DeviceIdPattern_values)
{
    DeviceIdPattern pat("abc_[1|17-18:71-72]_def_[0|731-732:246-247]_ghi");
    // DeviceIdPattern pat = DeviceIdPattern(
    //     {"abc_"sv, "_def_"sv, "_ghi"sv}, std::vector<unsigned>({1, 0}),
    //     std::vector<std::map<unsigned, unsigned>>(
    //         {{{17, 71}, {13, 31}}, {{731, 137}, {246, 642}}}));
    unsigned i = 0;
    for (const auto& v : pat.values())
    {
        switch (i)
        {
            case 0:
                EXPECT_EQ(v, "abc_71_def_246_ghi");
                break;
            case 1:
                EXPECT_EQ(v, "abc_72_def_246_ghi");
                break;
            case 2:
                EXPECT_EQ(v, "abc_71_def_247_ghi");
                break;
            case 3:
                EXPECT_EQ(v, "abc_72_def_247_ghi");
                break;
        }
        std::cout << v << std::endl;
        ++i;
    }
    EXPECT_EQ(i, 4);
    EXPECT_TRUE(pat.isInjective());
}

// TEST(DeviceIdTest, DeviceIdPattern_values)
// {
//     DeviceIdPattern pat("abc_[]_def_642_ghi");
//     DeviceIdPattern pat = DeviceIdPattern(
//         {"abc_"sv, "_def_"sv, "_ghi"sv}, std::vector<unsigned>({1, 0}),
//         std::vector<std::map<unsigned, unsigned>>(
//             {{{17, 71}, {13, 31}}, {{731, 137}, {246, 642}}}));
//     unsigned i = 0;
//     for (const auto& v : pat.values())
//     {
//         switch (i)
//         {
//             case 0:
//                 EXPECT_EQ(v, "abc_31_def_642_ghi");
//                 break;
//             case 1:
//                 EXPECT_EQ(v, "abc_71_def_642_ghi");
//                 break;
//             case 2:
//                 EXPECT_EQ(v, "abc_31_def_137_ghi");
//                 break;
//             case 3:
//                 EXPECT_EQ(v, "abc_71_def_137_ghi");
//                 break;
//         }
//         std::cout << v << std::endl;
//         ++i;
//     }
//     EXPECT_EQ(i, 4);
//     EXPECT_TRUE(pat.isInjective());
// }

TEST(DeviceIdTest, DeviceIdPattern_match_empty)
{
    DeviceIdPattern pat("PCIeRetimer");
    EXPECT_FALSE(pat.matches("GPU_SXM_2"));
    EXPECT_THAT(pat.match("GPU_SXM_2"), ElementsAre());
    EXPECT_FALSE(pat.matches(""));
    EXPECT_THAT(pat.match(""), ElementsAre());
    EXPECT_TRUE(pat.matches("PCIeRetimer"));
    EXPECT_THAT(pat.match("PCIeRetimer"), ElementsAre(PatternIndex()));
    EXPECT_TRUE(pat.isInjective());
}

TEST(DeviceIdTest, DeviceIdPattern_match_single)
{
    DeviceIdPattern pat("GPU_SXM_[1-8]");
    EXPECT_TRUE(pat.matches("GPU_SXM_2"));
    EXPECT_THAT(pat.match("GPU_SXM_2"), ElementsAre(PatternIndex(2)));
    EXPECT_TRUE(pat.matches("GPU_SXM_5"));
    EXPECT_THAT(pat.match("GPU_SXM_5"), ElementsAre(PatternIndex(5)));
    EXPECT_FALSE(pat.matches("GPU_SXM_10"));
    EXPECT_THAT(pat.match("GPU_SXM_10"), ElementsAre());
    EXPECT_FALSE(pat.matches("HGX_GPU_SXM_1"));
    EXPECT_THAT(pat.match("HGX_GPU_SXM_1"), ElementsAre());
    EXPECT_TRUE(pat.isInjective());
}

TEST(DeviceIdTest, DeviceIdPattern_match_double)
{
    DeviceIdPattern pat("GPU_SXM_[1-8]/NVLink_[0-39]");
    EXPECT_TRUE(pat.matches("GPU_SXM_4/NVLink_13"));
    EXPECT_THAT(pat.match("GPU_SXM_4/NVLink_13"),
                UnorderedElementsAre(PatternIndex(4, 13)));
    EXPECT_TRUE(pat.matches("GPU_SXM_1/NVLink_0"));
    EXPECT_THAT(pat.match("GPU_SXM_1/NVLink_0"),
                UnorderedElementsAre(PatternIndex(1, 0)));
    EXPECT_TRUE(pat.matches("GPU_SXM_8/NVLink_39"));
    EXPECT_THAT(pat.match("GPU_SXM_8/NVLink_39"),
                UnorderedElementsAre(PatternIndex(8, 39)));
    EXPECT_FALSE(pat.matches("GPU_SXM_9/NVLink_14"));
    EXPECT_THAT(pat.match("GPU_SXM_9/NVLink_14"), UnorderedElementsAre());
    EXPECT_TRUE(pat.isInjective());
}

// TEST(DeviceIdTest, DeviceIdPattern_match_noninjective)
// {
//     // "HSC_[0|0-3:8,4-7:9]";
//     DeviceIdPattern pat("HSC_[0|0-3:8,4-7:9]");
//     EXPECT_TRUE(pat.matches("HSC_8"));
//     EXPECT_THAT(pat.match("HSC_8"),
//                 UnorderedElementsAre(PatternIndex(0), PatternIndex(1),
//                                      PatternIndex(2), PatternIndex(3)));
//     EXPECT_TRUE(pat.matches("HSC_9"));
//     EXPECT_THAT(pat.match("HSC_9"),
//                 UnorderedElementsAre(PatternIndex(4), PatternIndex(5),
//                                      PatternIndex(6), PatternIndex(7)));
//     EXPECT_FALSE(pat.matches("HSC_0"));
//     EXPECT_THAT(pat.match("HSC_0"), UnorderedElementsAre());
//     EXPECT_FALSE(pat.isInjective());
// }

TEST(Test, tokenizer)
{
    using tokenizer = boost::tokenizer<boost::char_separator<char>>;
    std::string s = "Boost C++ Libraries";
    tokenizer tok{s};
    for (tokenizer::iterator it = tok.begin(); it != tok.end(); ++it)
        std::cout << *it << std::endl;
}

TEST(Test, splitting)
{
    using namespace boost::algorithm;
    std::string pattern{"GPU_SXM_[1-8]/NVLink_[0-39]"};
    std::vector<std::string> result;
    split(result, pattern, is_any_of("[]"));
    for (const auto& elem : result)
    {
        std::cout << "elem = '" << elem << "'" << std::endl;
    }
}

TEST(Test, splitting1)
{
    using namespace boost::algorithm;
    std::string pattern{"GPU_SXM_[1-8]"};
    std::vector<std::string> result;
    split(result, pattern, is_any_of("[]"));
    for (const auto& elem : result)
    {
        std::cout << "elem = '" << elem << "'" << std::endl;
    }
}

TEST(DeviceIdTest, DeviceIdPattern_separateTextAndBracketContents_single)
{
    std::vector<std::string> texts;
    std::vector<std::string> bracketContents;
    DeviceIdPattern::separateTextAndBracketContents("GPU_SXM_[1-8]", texts,
                                                    bracketContents);
    EXPECT_THAT(texts, ElementsAre("GPU_SXM_", ""));
    EXPECT_THAT(bracketContents, ElementsAre("1-8"));
}

TEST(DeviceIdTest, DeviceIdPattern_separateTextAndBracketContents_double)
{
    std::vector<std::string> texts;
    std::vector<std::string> bracketContents;
    DeviceIdPattern::separateTextAndBracketContents(
        "GPU_SXM_[1-8]/NVLink_[0-39]", texts, bracketContents);
    EXPECT_THAT(texts, ElementsAre("GPU_SXM_", "/NVLink_", ""));
    EXPECT_THAT(bracketContents, ElementsAre("1-8", "0-39"));
}

TEST(DeviceIdTest, DeviceIdPattern_separateTextAndBracketContents_none)
{
    std::vector<std::string> texts;
    std::vector<std::string> bracketContents;
    DeviceIdPattern::separateTextAndBracketContents("Baseboard", texts,
                                                    bracketContents);
    EXPECT_THAT(texts, ElementsAre("Baseboard"));
    EXPECT_THAT(bracketContents, ElementsAre());
}

TEST(DeviceIdTest, DeviceIdPattern_separateTextAndBracketContents_empty)
{
    std::vector<std::string> texts;
    std::vector<std::string> bracketContents;
    DeviceIdPattern::separateTextAndBracketContents("", texts, bracketContents);
    EXPECT_THAT(texts, ElementsAre(""));
    EXPECT_THAT(bracketContents, ElementsAre());
}

TEST(DeviceIdTest, DeviceIdPattern_full)
{
    DeviceIdPattern ip(
        "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_[1-8]");

    std::cout << "Testing index pattern \"" << ip << "\"" << std::endl;

    EXPECT_THAT(ip.domainVec(),
                ElementsAre(PatternIndex(1), PatternIndex(2), PatternIndex(3),
                            PatternIndex(4), PatternIndex(5), PatternIndex(6),
                            PatternIndex(7), PatternIndex(8)));

    EXPECT_THAT(
        ip.values(),
        ElementsAre(
            "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_1",
            "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_2",
            "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_3",
            "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_4",
            "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_5",
            "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_6",
            "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_7",
            "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_8"));

    EXPECT_EQ(ip(6),
              "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_6");
    EXPECT_EQ(ip(1),
              "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_1");
    EXPECT_EQ(ip(5, 7),
              "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_5");

    EXPECT_TRUE(ip.matches(
        "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_4"));
    EXPECT_FALSE(ip.matches(""));
    EXPECT_FALSE(ip.matches(
        "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_23"));

    EXPECT_THAT(
        ip.match("/xyz/openbmc_project/inventory/system/processors/GPU_SXM_2"),
        ElementsAre(PatternIndex(2)));
    EXPECT_THAT(
        ip.match("/xyz/openbmc_project/inventory/system/processors/GPU_SXM_4"),
        ElementsAre(PatternIndex(4)));
    EXPECT_THAT(ip.match("GPU_SXM_4"), ElementsAre());

    EXPECT_TRUE(ip.isInjective());

    EXPECT_EQ(ip.dim(), 1);

    // EXPECT_THAT(ip.dimDomain(0), ElementsAre(1, 2, 3, 4, 5, 6, 7, 8));
}

TEST(DeviceIdTest, PatternInputMapping_ZeroPattern)
{
    DeviceIdPattern ip("Critical");

    std::cout << "Testing index pattern \"" << ip << "\"" << std::endl;

    EXPECT_THAT(ip.domainVec(), ElementsAre(PatternIndex()));

    EXPECT_THAT(ip.values(), ElementsAre("Critical"));

    EXPECT_EQ(ip(6), "Critical");
    EXPECT_EQ(ip(1), "Critical");
    EXPECT_EQ(ip(5, 7), "Critical");
    EXPECT_EQ(ip(), "Critical");

    EXPECT_FALSE(ip.matches(
        "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_4"));
    EXPECT_FALSE(ip.matches(""));
    EXPECT_TRUE(ip.matches("Critical"));

    EXPECT_THAT(ip.match("Critical"), ElementsAre(PatternIndex()));
    EXPECT_THAT(ip.match("foo/bar"), ElementsAre());

    EXPECT_TRUE(ip.isInjective());
    EXPECT_EQ(ip.dim(), 0);
}

TEST(DeviceIdTest, all_event_info_strings)
{
    DeviceIdPattern ip("Critical");
    auto eventInfoStrings = {
        "",
        "/redfish/v1/Chassis/HGX_PCIeRetimer_[0-7]/Sensors/HGX_PCIeRetimer_[0-7]_Temp_0",
        "/redfish/v1/Chassis/HGX_PCIeSwitch_0/Sensors/HGX_PCIeSwitch_0_Temp_0",
        "/xyz/openbmc_project/GpioStatusHandler",
        "/xyz/openbmc_project/GpuOobRecovery",
        "/xyz/openbmc_project/inventory/system/chassis/HGX_Chassis_0",
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_[1-8]",
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_[0|1-8]/PCIeDevices/GPU_SXM_[0|1-8]",
        "/xyz/openbmc_project/inventory/system/chassis/HGX_NVSwitch_[0-3]",
        "/xyz/openbmc_project/inventory/system/chassis/HGX_NVSwitch_[0-3]/PCIeDevices/NVSwitch_[0|0-3]",
        "/xyz/openbmc_project/inventory/system/fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_[0-3]",
        "/xyz/openbmc_project/inventory/system/fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_[0-3]/Ports/NVLink_[0-39]",
        "/xyz/openbmc_project/inventory/system/fabrics/HGX_PCIeRetimerTopology_[0-7]/Switches/PCIeRetimer_[0|0-7]",
        "/xyz/openbmc_project/inventory/system/fabrics/HGX_PCIeRetimerTopology_[0-7]/Switches/PCIeRetimer_[0|0-7]/Ports/DOWN_0",
        "/xyz/openbmc_project/inventory/system/fabrics/HGX_PCIeSwitchTopology_0/Switches/PCIeSwitch_0/Ports/DOWN_[0-3]",
        "/xyz/openbmc_project/inventory/system/fabrics/HGX_PCIeSwitchTopology_0/Switches/PCIeSwitch_0/Ports/UP_0",
        "/xyz/openbmc_project/inventory/system/memory/GPU_SXM_[1-8]_DRAM_0",
        "/xyz/openbmc_project/inventory/system/processors/FPGA_0/Ports/PCIeToHMC_0",
        "/xyz/openbmc_project/inventory/system/processors/FPGA_0/Ports/PCIeToHMC_0 ",
        "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_[1-8]",
        "/xyz/openbmc_project/sensors/temperature/HGX_GPU_SXM_[1-8]_TEMP_0",
        "/xyz/openbmc_project/sensors/temperature/HGX_NVSwitch_[0-3]_TEMP_0",
        "/xyz/openbmc_project/software/HGX_FW_BMC_0",
        "/xyz/openbmc_project/software/HGX_FW_GPU_SXM_[1-8]",
        "/xyz/openbmc_project/software/HGX_FW_NVSwitch_[0-3]",
        "0",
        "0xfa1d",
        "1",
        "1024",
        "128",
        "16",
        "16384",
        "2",
        "2048",
        "256",
        "32",
        "32768",
        "4",
        "4096",
        "5",
        "512",
        "6",
        "64",
        "768",
        "769",
        "770",
        "774",
        "8",
        "8192",
        "AP0_BOOTCOMPLETE_TIMEOUT GPU_SXM_[1-8]",
        "AP0_BOOTCOMPLETE_TIMEOUT NVSwitch_[0-3]",
        "AP0_BOOTCOMPLETE_TIMEOUT PCIeSwitch_0",
        "AP0_PRIMARY_FW_AUTHENTICATION_STATUS GPU_SXM_[1-8]",
        "AP0_PRIMARY_FW_AUTHENTICATION_STATUS NVSwitch_[0-3]",
        "AP0_PRIMARY_FW_AUTHENTICATION_STATUS PCIeSwitch_0",
        "AP0_SPI_READ_FAILURE GPU_SXM_[1-8]",
        "AP0_SPI_READ_FAILURE NVSwitch_[0-3]",
        "AP0_SPI_READ_FAILURE PCIeSwitch_0",
        "Abnormal Power Change",
        "Abnormal Presence State Change",
        "Abnormal Speed Change",
        "Abnormal State Change",
        "Abnormal Width Change",
        "AbnormalPowerChange",
        "Absent",
        "ActiveWidth",
        "Alert caused by HSC alert",
        "BMC PCIe Uncorrectable Error",
        "BMC PCIe fatal error counts",
        "BackendErrorCode",
        "BaseBoard components will retry if encounters errors. If problem persists, analyze FPGA log or ERoT log.",
        "Baseboard GPU Presence change Interrupt",
        "Baseboard_0",
        "Baseboard_0 HSC Fault",
        "Boot completed failure",
        "CMDLINE",
        "Circuit breaker fault triggered or status is present is STATUS_OTHER byte",
        "Communication fault has occurred",
        "Cordon HGX server from the cluster for RMA evaluation.",
        "Cordon the HGX server node for further diagnosis.",
        "Cordon the HGX server node from the cluster for RMA evaluation.",
        "Critical",
        "CriticalHigh",
        "Current PCIe link speed",
        "Current PCIe link width",
        "Current Recovery Count",
        "Current Replay Count",
        "Current Temperature",
        "Current Training Error Count",
        "CurrentDeviceName",
        "CurrentSpeed",
        "DBUS",
        "DIRECT",
        "Data CRC Errors",
        "DataCRCCount",
        "Device is busy and could not respond to PMBus access",
        "DeviceCoreAPI",
        "Drain and Reset Recommended",
        "DrainAndResetRequired",
        "EEPROM Error",
        "EROT is in recovery mode, and HostBMC need to deassert EROT_RECOV and reboot the device. If proplem persists, cordon HGX server from the cluster for RMA evaluation.",
        "ERoT Fatal Abnormal State Change",
        "ERoT Recovery",
        "Error",
        "EventsPendingRegister",
        "FET is shorted or manufacture specific fault or warning has occurred",
        "FPGA - BMC PCIe Error",
        "FPGA - HMC PCIe Error",
        "FPGA HMC PCIe Correctable Error Count",
        "FPGA HMC PCIe Fatal Error Count",
        "FPGA HMC PCIe Non Fatal Error Count",
        "FPGA HMC PCIe UnCorrectable Error Count",
        "FPGA Temp Alert",
        "FPGA Temperature",
        "FPGA Thermal Parameter",
        "FPGA_0",
        "FPGA_NVSW[0-3]_EROT_RECOV_L NVSwitch_[0-3]",
        "FPGA_PEXSW_EROT_RECOV_L PCIeSwitch_0",
        "FPGA_SXM[0-7]_EROT_RECOV_L GPU_SXM_[1-8]",
        "FW Update with WP engaged - GPU",
        "FW Update with WP engaged - HMC",
        "FW Update with WP engaged - NVSwitch",
        "Failover to Redundant Interface(i2c)",
        "Flit CRC Errors",
        "FlitCRCCount",
        "ForceRestart",
        "GPU",
        "GPU Drain and Reset Recommended",
        "GPU Power Good Abnormal change",
        "GPU Reset Required",
        "GPU_SXM_[1-8]",
        "GPU_SXM_[1-8] gpu.xid.event XidTextMessage",
        "HMC SMBPBI Failover",
        "HMC {PCIe Uncorrectable Error}",
        "HMC_0",
        "HSC Power Good Abnormal change",
        "HSC detailed alert status",
        "HSC_[0-9]",
        "Hang",
        "HardShutdownHigh",
        "I2C EEPROM Error",
        "I2C Hang - Bus Timeout",
        "I2C Hang - FSM Timeout",
        "I2C3_ALERT",
        "I2C4_ALERT",
        "If WP enabled, Glacier/AP shall not be able to update SPI. This is a HW protection.",
        "Incorrect link speed",
        "Incorrect link width",
        "Increase the BaseBoard cooling and check thermal environmental to ensure Inlet Temperature reading is under UpperCritical threshold.",
        "Increase the BaseBoard cooling and check thermal environmental to ensure Inlet Temperature reading is under UpperCritical threshold. ",
        "Increase the BaseBoard cooling and check thermal environmental to ensure the GPU operates under UpperCritical threshold. ",
        "Increase the BaseBoard cooling and check thermal environmental to ensure the NVSwtich operates under UpperCritical threshold. ",
        "Inlet Sensor Temperature Alert",
        "Inlet Thermal Parameters",
        "Input voltage or current fault has occurred",
        "LTSSM Link Down",
        "LanesInUse",
        "Link data CRC error count",
        "Link flit CRC error count",
        "MCTP runtime failures",
        "NVLink data CRC error count",
        "NVLink flit error count",
        "NVLink recovery error count",
        "NVLink replay error count",
        "NVSW PRSNT state run time change",
        "NVSwitch",
        "NVSwitch 1V8 Abnormal change",
        "NVSwitch 3V3 Abnormal change",
        "NVSwitch 5V Abnormal change",
        "NVSwitch DVDD Abnormal change",
        "NVSwitch HVDD Abnormal change",
        "NVSwitch IBC Abnormal change",
        "NVSwitch Link Training Error",
        "NVSwitch PCIe link speed",
        "NVSwitch VDD Abnormal change",
        "NVSwitch_[0-3]",
        "NVSwitch_[0-3] nvswitch.Sxid.event XidTextMessage",
        "No immediate recovery required. If this event occurs multiple times in a short period, need to check other PCIe error status for further diagnostics.",
        "No immediate recovery required. Reset the GPU at next service window.",
        "No recovery required since FPGA will recover the fault automatically",
        "No recovery required.",
        "Not supported, always 0",
        "Nvlink Recovery Error",
        "Nvlink Replay Error",
        "OK",
        "One or more bits in MFR_SYSTEM_STATUS1 are set",
        "Output Overcurrent fault has occurred",
        "Output Overvoltage fault has occurred",
        "Output current/power fault or warning has occurred",
        "Output voltage fault or warning has occurred",
        "OverTemp",
        "PCB Thermal Warning",
        "PCIe LTSSM state",
        "PCIe Link Error - Downstream",
        "PCIe Link Error - Upstream",
        "PCIe Link Speed State Change",
        "PCIe Link Width State Change",
        "PCIe Switch 0V8 Abnormal Power change",
        "PCIe fatal error counts - Downstream",
        "PCIe fatal error counts - Upstream",
        "PCIe link error",
        "PCIe link goes down - Downstream",
        "PCIe link goes down - Upstream",
        "PCIe link speed state",
        "PCIe link width state",
        "PCIe non fatal error counts - Upstream",
        "PCIeRetimer 1V8VDD Abnormal Power Change",
        "PCIeRetimer PCIe Link Speed",
        "PCIeRetimer PCIe Link Width",
        "PCIeRetimer Power Change",
        "PCIeRetimer_[0-7]",
        "PCIeSwitch PCIe Link Speed",
        "PCIeSwitch PCIe Link Width",
        "PCIeSwitch_0",
        "PCIeType",
        "Physically Missing",
        "Power Good signal has been negated",
        "Power cycle the BaseBoard. If the problem persists, isolate the server for RMA evaluation",
        "Power cycle the BaseBoard. If the problem persists, isolate the server for RMA evaluation.",
        "Power off the BaseBoard within 1 second, and GPU will shutdown autonomously. Check thermal environmental to ensure the GPU operates under UpperFatal threshold and power cycle the Baseboard to recover.",
        "Power off the BaseBoard within 1 second, and the Retimer will be held in reset. Check thermal environmental to ensure the Retimer operates under UpperFatal threshold and power cycle the BaseBoard to recover. ",
        "Power off the BaseBoard within 1 second, the NVSwitch will be held in reset. Check thermal environmental to ensure the NVSwitch operates under UpperFatal threshold and power cycle the BaseBoard to recover. ",
        "Power off the BaseBoard within 1 second, the PCIe Switch will be held in reset. Check thermal environmental to ensure the PCIe Switch operates under UpperFatal threshold and power cycle the BaseBoard to recover. ",
        "Power off the BaseBoard within 1 second. Check thermal environmental to ensure FPGA operates under UpperFatal threshold and power cycle the BaseBoard to recover. ",
        "Presence",
        "RecoveryCount",
        "ReplayCount",
        "Reset Required",
        "Reset the GPU or power cycle the BaseBoard.  If problem persists, isolate the server for RMA evaluation.",
        "Reset the GPU or power cycle the BaseBoard. If problem persists, isolate the server for RMA evaluation.",
        "Reset the GPU. If the GPU continues to exhibit the problem, isolate the server for RMA evaluation.",
        "Reset the link. If problem persists, isolate the server for RMA evaluation.",
        "ResetRequired",
        "ResourceEvent.1.0.ResourceErrorThresholdExceeded",
        "ResourceEvent.1.0.ResourceErrorsDetected",
        "ResourceEvent.1.1.ResourceStateChanged",
        "Restart Fabric Manager or power cycle the BaseBoard. If problem persists, isolate the server for RMA evaluation.",
        "Retimer PWR_GD state run time change",
        "Row Remapping Count - Correctable",
        "Row Remapping Count - Uncorrectable",
        "Row Remapping Failure",
        "Row Remapping Failure Count",
        "Row Remapping Pending",
        "Row Remapping Pending Count",
        "Row-Remapping Pending",
        "RowRemappingFailureState",
        "RowRemappingPendingState",
        "SMBPBI Server Unavailable",
        "SMBPBIInterface",
        "SPI flash error",
        "SRAM ECC uncorrectable error count",
        "SXID Error",
        "SXID: {SXidErrMsg}",
        "SYS VR 1V 8 Abnormal Power change",
        "SYS VR 3V3 Abnormal Power change",
        "Secure boot failure",
        "SelfTest",
        "Server Unavailable",
        "Successful Recoveries",
        "Successful Replays",
        "Successful link recoveries",
        "Successful link replays",
        "System VR 1V8 Fault",
        "System VR 3V3 Fault",
        "THERM_OVERT",
        "Talk to glacier directly via PLDM vendor command to triage.",
        "Temperature fault or warning has occurred",
        "The MOSFET is not switched on for any reason or hot swap gate is off",
        "Thermal Warning",
        "ThrottleReason",
        "Throttled State",
        "Training Error",
        "TrainingError",
        "Unknown fault or warning has occurred",
        "Upper Critical Temperature",
        "Upper Critical Temperature Threshold",
        "Upper Fatal Temperature",
        "VIN under voltage fault has occurred",
        "VR Failure",
        "VR Fault",
        "VR Fault 0.9V",
        "VR Fault 1.8V",
        "Value",
        "VendorId",
        "Warning",
        "Width",
        "WriteProtected",
        "XID Error",
        "XID: {XidErrMsg}",
        "baseboard.pcb.temperature.alert",
        "ceCount",
        "ceRowRemappingCount",
        "com.nvidia.MemoryRowRemapping",
        "com.nvidia.SMPBI",
        "eROT will try to recover AP by a reset/reboot. If there is still a problem, AP FW needs to be re-flashed.",
        "false",
        "feCount",
        "fpga.thermal.alert",
        "fpga.thermal.temperature.singlePrecision",
        "fpga_regtbl_wrapper",
        "gpu.interrupt.PresenceInfo",
        "gpu.interrupt.erot",
        "gpu.interrupt.powerGoodAbnormalChange",
        "gpu.nvlink.recoveryErrCount",
        "gpu.nvlink.replayErrCount",
        "gpu.thermal.alert",
        "gpu.thermal.temperature.overTemperatureInfo",
        "hmc.interrupt.erot",
        "hostBmc.pcie.linkStatus.page5",
        "hsc.device.alert",
        "hsc.power.abnormalPowerChange",
        "inletTemp.thermal.alert",
        "mctp-error-detection",
        "mctp-vdm-util-wrapper",
        "nonfeCount",
        "nvswitch.1V8.abnormalPowerChange",
        "nvswitch.3V3.abnormalPowerChange",
        "nvswitch.5V.abnormalPowerChange",
        "nvswitch.device.abnormalPresenceChange",
        "nvswitch.device.vrFault",
        "nvswitch.dvdd.abnormalPowerChange",
        "nvswitch.hvdd.abnormalPowerChange",
        "nvswitch.interrupt.erot",
        "nvswitch.thermal.alert",
        "nvswitch.thermal.temperature.overTemperatureInfo",
        "nvswitch.vdd.abnormalPowerChange",
        "pcie",
        "pcieretimer.0V9.abnormalPowerChange",
        "pcieretimer.1V8VDD.abnormalPowerChange",
        "pcieretimer.thermal.temperature.overTemperatureInfo",
        "pcieswitch.0V8.abnormalPowerChange",
        "pcieswitch.interrupt.erot",
        "pcieswitch.pcie.linkStatus.page6",
        "pcieswitch.thermal.temperature.overTemperatureInfo",
        "range",
        "sysvr1v8.power.abnormalPowerChange",
        "sysvr3v3.power.abnormalPowerChange",
        "true",
        "ueCount",
        "ueRowRemappingCount",
        "xid-event-util-wrapper",
        "xyz.openbmc_project.GpioStatus",
        "xyz.openbmc_project.GpuOobRecovery.Server",
        "xyz.openbmc_project.Inventory.Item.PCIeDevice",
        "xyz.openbmc_project.Inventory.Item.PCIeDevice.PCIeTypes.Gen5",
        "xyz.openbmc_project.Inventory.Item.Port",
        "xyz.openbmc_project.Inventory.Item.Switch",
        "xyz.openbmc_project.Memory.MemoryECC",
        "xyz.openbmc_project.PCIe.PCIeECC",
        "xyz.openbmc_project.Sensor.Threshold.Critical",
        "xyz.openbmc_project.Sensor.Threshold.HardShutdown",
        "xyz.openbmc_project.Sensor.Value",
        "xyz.openbmc_project.Software.Settings",
        "xyz.openbmc_project.State.PowerChange",
        "xyz.openbmc_project.State.PresenceState",
        "xyz.openbmc_project.State.ProcessorPerformance",
        "xyz.openbmc_project.State.ProcessorPerformance.ThrottleReasons.None",
        "xyz.openbmc_project.State.ResetStatus",
        "xyz.openbmc_project.com.nvidia.Events.PendingRegister",
        "{APId} Firmware",
        "{ERoTId} ERoT_Fatal",
        "{ERoTId} Firmware",
        "{FPGAId} PCIe",
        "{FPGAId} Temperature",
        "{GPUId}",
        "{GPUId} Driver Event Message",
        "{GPUId} I2C",
        "{GPUId} Memory",
        "{GPUId} PCIe",
        "{GPUId} PowerGood",
        "{GPUId} Temperature",
        "{GPUId} {NVLinkId}",
        "{HMCId} Firmware",
        "{HMCId} SMBPBI",
        "{HSCId} Alert",
        "{HSCId} PowerGood",
        "{InletTempId} Temperature",
        "{NVSwitchId}",
        "{NVSwitchId} Driver Event Message",
        "{NVSwitchId} I2C",
        "{NVSwitchId} PCIe",
        "{NVSwitchId} PowerGood",
        "{NVSwitchId} SMBPBI",
        "{NVSwitchId} Temperature",
        "{NVSwitchId} {NVLinkId}",
        "{PCIe Uncorrectable Error}",
        "{PCIeRetimerId} Firmware",
        "{PCIeRetimerId} PCIe",
        "{PCIeRetimerId} PowerGood",
        "{PCIeRetimerId} Temperature",
        "{PCIeSwitchId} PCIe",
        "{PCIeSwitchId} PowerGood",
        "{PCIeSwitchId} Temperature",
        "{SysVRId} PowerGood",
        "{UpperCritical Threshold}",
        "{UpperFatal Threshold}"};

    for (const auto& str : eventInfoStrings)
    {
        try
        {
            DeviceIdPattern pat(str);
            std::cout << "'" << str << "': " << std::endl;
            for (const auto& arg : pat.domain())
            {
                std::cout << arg << ": '" << pat.eval(arg) << "'" << std::endl;
            }
            std::cout << std::endl;
        }
        catch (const std::exception& e)
        {
            EXPECT_NO_THROW({ DeviceIdPattern pat(str); });
        }
    }
}

namespace syntax
{

TEST(DeviceIdTest, parseNonNegativeInt)
{
    EXPECT_EQ(parseNonNegativeInt("123"sv), 123u);
    EXPECT_EQ(parseNonNegativeInt(std::string("0")), 0u);
    EXPECT_ANY_THROW(parseNonNegativeInt(""sv));
    EXPECT_ANY_THROW(parseNonNegativeInt("-12"sv));
    EXPECT_ANY_THROW(parseNonNegativeInt("-1.2"sv));
    EXPECT_ANY_THROW(parseNonNegativeInt(std::string("ab")));
    EXPECT_ANY_THROW(parseNonNegativeInt(std::string("  42")));
    EXPECT_ANY_THROW(parseNonNegativeInt(std::string("42  ")));
    EXPECT_ANY_THROW(parseNonNegativeInt(std::string("42othertrash")));
}

TEST(DeviceIdTest, BracketRange_parse)
{
    EXPECT_EQ(BracketRange::parse("123"sv), BracketRange(123, 123));
    EXPECT_EQ(BracketRange::parse(std::string("0")), BracketRange(0, 0));
    EXPECT_ANY_THROW(BracketRange::parse(""sv));
    EXPECT_ANY_THROW(BracketRange::parse("-12"sv));
    EXPECT_ANY_THROW(BracketRange::parse("-1.2"sv));
    EXPECT_ANY_THROW(BracketRange::parse(std::string("ab")));
    EXPECT_ANY_THROW(BracketRange::parse(std::string("  42")));
    EXPECT_ANY_THROW(BracketRange::parse(std::string("42  ")));
    EXPECT_ANY_THROW(BracketRange::parse(std::string("42othertrash")));

    EXPECT_EQ(BracketRange::parse("2-8"sv), BracketRange(2, 8));
    EXPECT_EQ(BracketRange::parse("0-10"sv), BracketRange(0, 10));
    EXPECT_EQ(BracketRange::parse("0"sv), BracketRange(0, 0));
    EXPECT_EQ(BracketRange::parse("5"sv), BracketRange(5, 5));
    EXPECT_EQ(BracketRange::parse("3-3"sv), BracketRange(3, 3));
    EXPECT_ANY_THROW(BracketRange::parse("0--5"sv));
    EXPECT_ANY_THROW(BracketRange::parse(""sv));
    EXPECT_ANY_THROW(BracketRange::parse("trash"sv));
    EXPECT_ANY_THROW(BracketRange::parse("0- 10"sv));
    EXPECT_ANY_THROW(BracketRange::parse("7-4"sv));
}

TEST(DeviceIdTest, BracketRangeMap_parse)
{
    EXPECT_EQ(BracketRangeMap::parse("123"sv),
              BracketRangeMap(BracketRange(123, 123), BracketRange(123, 123)));
    EXPECT_EQ(BracketRangeMap::parse(std::string("0")),
              BracketRangeMap(BracketRange(0, 0), BracketRange(0, 0)));
    EXPECT_ANY_THROW(BracketRangeMap::parse(""sv));
    EXPECT_ANY_THROW(BracketRangeMap::parse("-12"sv));
    EXPECT_ANY_THROW(BracketRangeMap::parse("-1.2"sv));
    EXPECT_ANY_THROW(BracketRangeMap::parse(std::string("ab")));
    EXPECT_ANY_THROW(BracketRangeMap::parse(std::string("  42")));
    EXPECT_ANY_THROW(BracketRangeMap::parse(std::string("42  ")));
    EXPECT_ANY_THROW(BracketRangeMap::parse(std::string("42othertrash")));

    EXPECT_EQ(BracketRangeMap::parse("2-8"sv),
              BracketRangeMap(BracketRange(2, 8), BracketRange(2, 8)));
    EXPECT_EQ(BracketRangeMap::parse("0-10"sv),
              BracketRangeMap(BracketRange(0, 10), BracketRange(0, 10)));
    EXPECT_EQ(BracketRangeMap::parse("0"sv),
              BracketRangeMap(BracketRange(0, 0), BracketRange(0, 0)));
    EXPECT_EQ(BracketRangeMap::parse("5"sv),
              BracketRangeMap(BracketRange(5, 5), BracketRange(5, 5)));
    EXPECT_EQ(BracketRangeMap::parse("3-3"sv),
              BracketRangeMap(BracketRange(3, 3), BracketRange(3, 3)));
    EXPECT_ANY_THROW(BracketRangeMap::parse("0--5"sv));
    EXPECT_ANY_THROW(BracketRangeMap::parse(""sv));
    EXPECT_ANY_THROW(BracketRangeMap::parse("trash"sv));
    EXPECT_ANY_THROW(BracketRangeMap::parse("0- 10"sv));
    EXPECT_ANY_THROW(BracketRangeMap::parse("7-4"sv));

    EXPECT_EQ(BracketRangeMap::parse("1"),
              BracketRangeMap(BracketRange(1, 1), BracketRange(1, 1)));
    EXPECT_EQ(BracketRangeMap::parse("0-3"sv),
              BracketRangeMap(BracketRange(0, 3), BracketRange(0, 3)));
    EXPECT_EQ(BracketRangeMap::parse("1-39"sv),
              BracketRangeMap(BracketRange(1, 39), BracketRange(1, 39)));
    EXPECT_EQ(BracketRangeMap::parse("0-3:0-3"sv),
              BracketRangeMap(BracketRange(0, 3), BracketRange(0, 3)));
    EXPECT_EQ(BracketRangeMap::parse(std::string("0-3:10")),
              BracketRangeMap(BracketRange(0, 3), BracketRange(10, 10)));
    EXPECT_EQ(BracketRangeMap::parse("0-7:1-8"sv),
              BracketRangeMap(BracketRange(0, 7), BracketRange(1, 8)));
    EXPECT_ANY_THROW(BracketRangeMap::parse("0-7:0-3"sv));
    EXPECT_ANY_THROW(BracketRangeMap::parse("0-7: 1-8"sv));
    EXPECT_ANY_THROW(BracketRangeMap::parse("0-7  :1-8"sv));
    EXPECT_ANY_THROW(BracketRangeMap::parse(" 0-7:1-8"sv));
    EXPECT_ANY_THROW(BracketRangeMap::parse("0-7:1-8 "sv));
    EXPECT_ANY_THROW(BracketRangeMap::parse("0-7::1-8"sv));
}

TEST(DeviceIdTest, parseBracketMap)
{
    // marcinw:TODO: test against explicit maps
    EXPECT_EQ(
        parseBracketMap("1"),
        (BracketMap)BracketRangeMap(BracketRange(1, 1), BracketRange(1, 1)));
    EXPECT_EQ(
        parseBracketMap("0-3"sv),
        (BracketMap)BracketRangeMap(BracketRange(0, 3), BracketRange(0, 3)));
    EXPECT_EQ(
        parseBracketMap("1-39"sv),
        (BracketMap)BracketRangeMap(BracketRange(1, 39), BracketRange(1, 39)));
    EXPECT_EQ(
        parseBracketMap("0-3:0-3"sv),
        (BracketMap)BracketRangeMap(BracketRange(0, 3), BracketRange(0, 3)));
    EXPECT_EQ(
        parseBracketMap(std::string("0-3:10")),
        (BracketMap)BracketRangeMap(BracketRange(0, 3), BracketRange(10, 10)));
    EXPECT_EQ(
        parseBracketMap("0-7:1-8"sv),
        (BracketMap)BracketRangeMap(BracketRange(0, 7), BracketRange(1, 8)));
    EXPECT_ANY_THROW(parseBracketMap("0-7:0-3"sv));
    EXPECT_ANY_THROW(parseBracketMap("0-7: 1-8"sv));
    EXPECT_ANY_THROW(parseBracketMap("0-7  :1-8"sv));
    EXPECT_ANY_THROW(parseBracketMap(" 0-7:1-8"sv));
    EXPECT_ANY_THROW(parseBracketMap("0-7:1-8 "sv));
    EXPECT_ANY_THROW(parseBracketMap("0-7::1-8"sv));
    // marcinw:TODO: all the non-degenerated cases
}

TEST(DeviceIdTest, IndexedBracketMap)
{
    EXPECT_EQ(IndexedBracketMap::parse("123"sv),
              IndexedBracketMap(BracketRangeMap(BracketRange(123, 123),
                                                BracketRange(123, 123))));
    EXPECT_EQ(IndexedBracketMap::parse(std::string("0")),
              IndexedBracketMap(
                  BracketRangeMap(BracketRange(0, 0), BracketRange(0, 0))));
    EXPECT_ANY_THROW(IndexedBracketMap::parse(""sv));
    EXPECT_ANY_THROW(IndexedBracketMap::parse("-12"sv));
    EXPECT_ANY_THROW(IndexedBracketMap::parse("-1.2"sv));
    EXPECT_ANY_THROW(IndexedBracketMap::parse(std::string("ab")));
    EXPECT_ANY_THROW(IndexedBracketMap::parse(std::string("  42")));
    EXPECT_ANY_THROW(IndexedBracketMap::parse(std::string("42  ")));
    EXPECT_ANY_THROW(IndexedBracketMap::parse(std::string("42othertrash")));

    EXPECT_EQ(IndexedBracketMap::parse("2-8"sv),
              IndexedBracketMap(
                  BracketRangeMap(BracketRange(2, 8), BracketRange(2, 8))));
    EXPECT_EQ(IndexedBracketMap::parse("0-10"sv),
              IndexedBracketMap(
                  BracketRangeMap(BracketRange(0, 10), BracketRange(0, 10))));
    EXPECT_EQ(IndexedBracketMap::parse("0"sv),
              IndexedBracketMap(
                  BracketRangeMap(BracketRange(0, 0), BracketRange(0, 0))));
    EXPECT_EQ(IndexedBracketMap::parse("5"sv),
              IndexedBracketMap(
                  BracketRangeMap(BracketRange(5, 5), BracketRange(5, 5))));
    EXPECT_EQ(IndexedBracketMap::parse("3-3"sv),
              IndexedBracketMap(
                  BracketRangeMap(BracketRange(3, 3), BracketRange(3, 3))));
    EXPECT_ANY_THROW(IndexedBracketMap::parse("0--5"sv));
    EXPECT_ANY_THROW(IndexedBracketMap::parse(""sv));
    EXPECT_ANY_THROW(IndexedBracketMap::parse("trash"sv));
    EXPECT_ANY_THROW(IndexedBracketMap::parse("0- 10"sv));
    EXPECT_ANY_THROW(IndexedBracketMap::parse("7-4"sv));

    EXPECT_EQ(IndexedBracketMap::parse("1"),
              IndexedBracketMap(
                  BracketRangeMap(BracketRange(1, 1), BracketRange(1, 1))));
    EXPECT_EQ(IndexedBracketMap::parse("0-3"sv),
              IndexedBracketMap(
                  BracketRangeMap(BracketRange(0, 3), BracketRange(0, 3))));
    EXPECT_EQ(IndexedBracketMap::parse("1-39"sv),
              IndexedBracketMap(
                  BracketRangeMap(BracketRange(1, 39), BracketRange(1, 39))));
    EXPECT_EQ(IndexedBracketMap::parse("0-3:0-3"sv),
              IndexedBracketMap(
                  BracketRangeMap(BracketRange(0, 3), BracketRange(0, 3))));
    EXPECT_EQ(IndexedBracketMap::parse(std::string("0-3:10")),
              IndexedBracketMap(
                  BracketRangeMap(BracketRange(0, 3), BracketRange(10, 10))));
    EXPECT_EQ(IndexedBracketMap::parse("0-7:1-8"sv),
              IndexedBracketMap(
                  BracketRangeMap(BracketRange(0, 7), BracketRange(1, 8))));
    EXPECT_ANY_THROW(IndexedBracketMap::parse("0-7:0-3"sv));
    EXPECT_ANY_THROW(IndexedBracketMap::parse("0-7: 1-8"sv));
    EXPECT_ANY_THROW(IndexedBracketMap::parse("0-7  :1-8"sv));
    EXPECT_ANY_THROW(IndexedBracketMap::parse(" 0-7:1-8"sv));
    EXPECT_ANY_THROW(IndexedBracketMap::parse("0-7:1-8 "sv));
    EXPECT_ANY_THROW(IndexedBracketMap::parse("0-7::1-8"sv));

    EXPECT_EQ(IndexedBracketMap::parse("0|123"sv),
              IndexedBracketMap(0, BracketRangeMap(BracketRange(123, 123),
                                                   BracketRange(123, 123))));
    EXPECT_EQ(IndexedBracketMap::parse(std::string("5|0")),
              IndexedBracketMap(
                  5, BracketRangeMap(BracketRange(0, 0), BracketRange(0, 0))));
    EXPECT_EQ(IndexedBracketMap::parse("1|2-8"sv),
              IndexedBracketMap(
                  1, BracketRangeMap(BracketRange(2, 8), BracketRange(2, 8))));
    EXPECT_EQ(IndexedBracketMap::parse("8|0-10"sv),
              IndexedBracketMap(8, BracketRangeMap(BracketRange(0, 10),
                                                   BracketRange(0, 10))));
    EXPECT_EQ(IndexedBracketMap::parse(std::string("0|0-3:10")),
              IndexedBracketMap(0, BracketRangeMap(BracketRange(0, 3),
                                                   BracketRange(10, 10))));
    EXPECT_EQ(IndexedBracketMap::parse("1|0-7:1-8"sv),
              IndexedBracketMap(
                  1, BracketRangeMap(BracketRange(0, 7), BracketRange(1, 8))));

    EXPECT_ANY_THROW(IndexedBracketMap::parse("1| 0-7:1-8"sv));
    EXPECT_ANY_THROW(IndexedBracketMap::parse("-1|0-7:1-8"sv));
    EXPECT_ANY_THROW(IndexedBracketMap::parse("abc|0-7:1-8"sv));
    EXPECT_ANY_THROW(IndexedBracketMap::parse("0/0-7:1-8"sv));
    EXPECT_ANY_THROW(IndexedBracketMap::parse("5:0-7:1-8"sv));
}

} // namespace syntax

TEST(DeviceIdTest, JsonPattern_eval)
{
    nlohmann::json js{
        {"type", "DBUS"},
        {"object", "/.../Switches/NVSwitch_[0-3]/Ports/NVLink_[0-39]"},
        {"interface", "xyz.openbmc_project.Inventory.Item.Port"},
        {"property", "FlitCRCCount"},
        {"check", {{"not_equal", "0"}}}};
    JsonPattern jsp(js);
    EXPECT_EQ(jsp.eval(PatternIndex(1, 12)),
              nlohmann::json(
                  {{"type", "DBUS"},
                   {"object", "/.../Switches/NVSwitch_1/Ports/NVLink_12"},
                   {"interface", "xyz.openbmc_project.Inventory.Item.Port"},
                   {"property", "FlitCRCCount"},
                   {"check", {{"not_equal", "0"}}}}));
    EXPECT_EQ(jsp.eval(PatternIndex(3, 0)),
              nlohmann::json(
                  {{"type", "DBUS"},
                   {"object", "/.../Switches/NVSwitch_3/Ports/NVLink_0"},
                   {"interface", "xyz.openbmc_project.Inventory.Item.Port"},
                   {"property", "FlitCRCCount"},
                   {"check", {{"not_equal", "0"}}}}));
    EXPECT_ANY_THROW(jsp.eval(PatternIndex(3)));
    EXPECT_ANY_THROW(jsp.eval(PatternIndex()));
    EXPECT_ANY_THROW(jsp.eval(PatternIndex(4, 0)));
}

TEST(DeviceIdTest, JsonPattern_eval1)
{
    nlohmann::json js{
        {"message_id", "ResourceEvent.1.0.ResourceErrorsDetected"},
        {"message_args",
         {{"patterns",
           nlohmann::json::array(
               {"NVSwitch_[0|0-3] NVLink_[1|0-17]", "Flit CRC Errors"})},
          {"parameters", nlohmann::json::array()}}}};
    JsonPattern jsp(js);
    EXPECT_EQ(jsp.eval(PatternIndex(1, 12)),
              nlohmann::json(
                  {{"message_id", "ResourceEvent.1.0.ResourceErrorsDetected"},
                   {"message_args",
                    {{"patterns", nlohmann::json::array({"NVSwitch_1 NVLink_12",
                                                         "Flit CRC Errors"})},
                     {"parameters", nlohmann::json::array()}}}}));
    EXPECT_EQ(jsp.eval(PatternIndex(0, 1)),
              nlohmann::json(
                  {{"message_id", "ResourceEvent.1.0.ResourceErrorsDetected"},
                   {"message_args",
                    {{"patterns", nlohmann::json::array({"NVSwitch_0 NVLink_1",
                                                         "Flit CRC Errors"})},
                     {"parameters", nlohmann::json::array()}}}}));
    EXPECT_ANY_THROW(jsp.eval(PatternIndex(3)));
    EXPECT_ANY_THROW(jsp.eval(PatternIndex()));
    EXPECT_ANY_THROW(jsp.eval(PatternIndex(4, 0)));
}

} // namespace device_id
