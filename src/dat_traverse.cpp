
/*
 *
 */

#include "dat_traverse.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <ostream>
#include <queue>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace dat_traverse
{

/* test */

Device::~Device()
{}

void Device::populateMap(std::map<std::string, dat_traverse::Device>& m,
                         const std::string& file)
{

    std::ifstream i(file);
    json j;
    i >> j;

    for (const auto& el : j.items())
    {
        auto deviceName = el.key();
        dat_traverse::Device device(deviceName, el.value());
        m.insert(
            std::pair<std::string, dat_traverse::Device>(deviceName, device));
    }

    // fill out parents of all child devices on 2nd pass
    for (const auto& entry : m)
    {
        for (const auto& child : entry.second.association)
        {
            m.at(child).parents.push_back(entry.first);
        }
    }
}

void Device::printTree(const std::map<std::string, dat_traverse::Device>& m)
{
    std::queue<std::string> fringe;
    fringe.push("Baseboard");
    while (!fringe.empty())
    {
        std::string deviceName = fringe.front();
        fringe.pop();

        dat_traverse::Device device = m.at(deviceName);

        std::cerr << "Found Device " << deviceName << "\n";
        for (const auto& parent : device.parents)
        {
            std::cerr << parent << " is a parent of " << deviceName << "\n";
        }

        for (const auto& child : device.association)
        {
            std::cerr << "Found child " << child << " of device " << deviceName
                      << "\n";
            fringe.push(child);
        }
        std::cerr << "\n";
    }
}

Device::Device(const std::string& name)
{
    this->name = name;
}

Device::Device(const std::string& name, const json& j)
{

    this->name = name;
    this->association = j["association"].get<std::vector<std::string>>();

    std::list<std::string> layers = {"power_rail",      "erot_control",
                                     "pin_status",      "interface_status",
                                     "protocol_status", "firmware_status"};

    std::map<std::string, dat_traverse::TestLayer> test;

    for (const auto& layer : layers)
    {
        std::map<std::string, dat_traverse::TestPoint> testPoints;
        for (const auto& point : j[layer])
        {
            dat_traverse::TestPoint tp;
            tp.accessor = point["accessor"];
            tp.expectedValue = point["expected_value"].get<std::string>();
            std::string name = point["name"].get<std::string>();
            testPoints.insert(
                std::pair<std::string, dat_traverse::TestPoint>(name, tp));
        }

        dat_traverse::TestLayer testLayer;
        testLayer.testPoints = testPoints;
        test.insert(
            std::pair<std::string, dat_traverse::TestLayer>(layer, testLayer));
    }
    this->test = test;
}

} // namespace dat_traverse
