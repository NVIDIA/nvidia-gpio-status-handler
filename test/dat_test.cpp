#include "gmock/gmock.h"
#include "dat_traverse.hpp"
#include "nlohmann/json.hpp"
#include "algorithm"
#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <exception>

TEST(DatTest, AssociationPopulated)
{
    nlohmann::json j;
    j["association"] = {"GPU1", "GPU2"};
    dat_traverse::Device device("Baseboard", j);
    EXPECT_EQ(device.association.size(), 2);
}

TEST(DatTest, NamePopulated)
{
    nlohmann::json j;
    j["association"] = {"GPU1", "GPU2"};
    dat_traverse::Device device("Baseboard", j);
    EXPECT_EQ(device.name, "Baseboard");
}

TEST(DatTest, MapPopulated)
{
    nlohmann::json j;
    j["Baseboard"]["association"] = {"GPU1", "GPU2"};
    j["GPU1"]["association"] = {"HSC1", "HSC2"};
    std::vector<std::string> v = j["Baseboard"]["association"].get<std::vector<std::string>>();
    EXPECT_EQ(std::find(v.begin(), v.end(), "GPU1") != v.end(), true);
}
