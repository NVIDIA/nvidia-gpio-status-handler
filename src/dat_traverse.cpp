
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

void Device::populateMap(std::map<std::string, dat_traverse::Device>& dat,
                         const std::string& file)
{
    std::ifstream i(file);
    json j;
    i >> j;

    for (const auto& el : j.items())
    {
        auto deviceName = el.key();
        dat_traverse::Device device(deviceName, el.value());
        dat.insert(
            std::pair<std::string, dat_traverse::Device>(deviceName, device));
    }

    // fill out parents of all child devices on 2nd pass
    for (const auto& entry : dat)
    {
        for (const auto& child : entry.second.association)
        {
            dat.at(child).parents.push_back(entry.first);
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
    this->healthStatus.health = "OK";
    this->healthStatus.healthRollup = "OK";
    this->healthStatus.originOfCondition = "";
    this->healthStatus.triState = "Active";

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

namespace event_handler
{

DATTraverse::~DATTraverse()
{}

void DATTraverse::printBranch(
    const std::map<std::string, dat_traverse::Device>& dat,
    const std::vector<std::string>& devices)
{
    for (const auto& dev : devices)
    {
        std::cerr << dev << ":\n";
        auto device = dat.at(dev);
        std::cerr << "Health: " << device.healthStatus.health << "\n";
        std::cerr << "Healthrollup: " << device.healthStatus.healthRollup
                  << "\n";
        std::cerr << "OOC: " << device.healthStatus.originOfCondition << "\n";
        std::cerr << "State: " << device.healthStatus.triState << "\n\n";
    }
}

void DATTraverse::setDAT(const std::map<std::string, dat_traverse::Device>& dat)
{
    this->dat = dat;
}

bool DATTraverse::hasParents(const dat_traverse::Device& device)
{
    return device.parents.size() > 0;
}

bool DATTraverse::checkHealth(const dat_traverse::Device& device)
{
    return device.healthStatus.health == std::string("OK");
}

void DATTraverse::setHealthProperties(dat_traverse::Device& targetDevice,
                                      const dat_traverse::Status& status)
{
    targetDevice.healthStatus.healthRollup = status.healthRollup;
    targetDevice.healthStatus.triState = status.triState;
}

void DATTraverse::setOriginOfCondition(dat_traverse::Device& targetDevice,
                                       const dat_traverse::Status& status)
{
    targetDevice.healthStatus.originOfCondition = status.originOfCondition;
}

std::vector<std::string> DATTraverse::getSubAssociations(
    std::map<std::string, dat_traverse::Device>& dat,
    const std::string& device)
{
    std::queue<std::string> fringe;
    std::vector<std::string> childVec;
    fringe.push(device);
    childVec.push_back(device);

    while (!fringe.empty())
    {
        std::string deviceName = fringe.front();
        fringe.pop();
        const dat_traverse::Device& node = dat.at(deviceName);

        for (const auto& child : node.association)
        {
            fringe.push(child);
            childVec.push_back(child);
        }
    }

    return childVec;
}

void DATTraverse::parentTraverse(
    std::map<std::string, dat_traverse::Device>& dat, const std::string& device,
    const std::function<bool(const dat_traverse::Device& device)> comparator,
    const std::vector<std::function<void(dat_traverse::Device& device,
                                         const dat_traverse::Status& status)>>
        &action)
{

    dat_traverse::Device& dev = dat.at(device);

    dat_traverse::Status status;
    status.health = dev.healthStatus.health;
    status.healthRollup = dev.healthStatus.health;
    status.originOfCondition = device;
    status.triState = dev.healthStatus.triState;

    std::queue<std::string> fringe;
    fringe.push(device);

    while (!fringe.empty())
    {
        std::string deviceName = fringe.front();
        fringe.pop();

        dat_traverse::Device& node = dat.at(deviceName);

        for (const auto& callback : action)
        {
            callback(node, status);
        }

        if (comparator(node))
        {
            for (const auto& parent : node.parents)
            {
                fringe.push(parent);
            }
        }
    }
}

} // namespace event_handler
