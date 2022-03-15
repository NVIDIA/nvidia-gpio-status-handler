#include "dat_traverse.hpp"
#include "nlohmann/json.hpp"

#include <exception>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "algorithm"

#include "gmock/gmock.h"

TEST(DatTest, AssociationPopulated)
{
    nlohmann::json j;
    j["association"] = {"GPU1", "GPU2"};
    j["power_rail"] = nlohmann::json::array();
    j["erot_control"] = nlohmann::json::array();
    j["pin_status"] = nlohmann::json::array();
    j["interface_status"] = nlohmann::json::array();
    j["firmware_status"] = nlohmann::json::array();
    j["protocol_status"] = nlohmann::json::array();
    dat_traverse::Device device("Baseboard", j);
    EXPECT_EQ(device.association.size(), 2);
}

TEST(DatTest, NamePopulated)
{
    nlohmann::json j;
    j["association"] = {"GPU1", "GPU2"};
    j["power_rail"] = nlohmann::json::array();
    j["erot_control"] = nlohmann::json::array();
    j["pin_status"] = nlohmann::json::array();
    j["interface_status"] = nlohmann::json::array();
    j["firmware_status"] = nlohmann::json::array();
    j["protocol_status"] = nlohmann::json::array();
    dat_traverse::Device device("Baseboard", j);
    EXPECT_EQ(device.name, "Baseboard");
}

TEST(DatTest, MapPopulated)
{
    nlohmann::json j;
    j["Baseboard"]["association"] = {"GPU1", "GPU2"};
    j["GPU1"]["association"] = {"HSC1", "HSC2"};

    std::vector<std::string> v =
        j["Baseboard"]["association"].get<std::vector<std::string>>();
    EXPECT_EQ(std::find(v.begin(), v.end(), "GPU1") != v.end(), true);
}
