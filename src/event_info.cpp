/*
 *
 */

#include "event_info.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace event_info
{

void loadFromFile(EventMap& eventMap, const std::string& file)
{

    std::cout << "loadFromFile func (" << file << ").\n";
    std::ifstream i(file);
    json j;
    i >> j;

    for (const auto& el : j.items())
    {
        auto deviceType = el.key();
        std::vector<event_info::EventNode> v = {};

        for (const auto& event : el.value())
        {
            std::cout << "create event (" << event["event"] << ").\n";

            event_info::EventNode eventNode(event["event"]);

            std::cout << "load event (" << event["event"] << ").\n";
            eventNode.loadFrom(event);

            std::cout << "push event (" << event["event"] << ").\n";
            v.push_back(eventNode);
        }
        eventMap.insert(
            std::pair<std::string, std::vector<event_info::EventNode>>(
                deviceType, v));
    }
}

void printMap(const EventMap& eventMap)
{
    for (const auto& dev : eventMap)
    {
        std::cerr << dev.first << " events:\n";

        for (const auto& event : dev.second)
        {
            std::cerr << event.event << "\n";
        }

        std::cerr << "\n";
    }
}

void EventNode::loadFrom(const json& j)
{
    this->event = j["event"];
    this->deviceType = j["device_type"];
    this->triggerCount = j["trigger_count"].get<int>();
    this->eventTrigger = j["event_trigger"];
    this->telemetries = j["telemetries"].get<std::vector<std::string>>();
    this->action = j["action"];
    this->device = "";

    this->counterReset = j["event_counter_reset"];

    this->messageRegistry = {j["redfish"]["message_id"].get<std::string>(),
                             {j["severity"], j["resolution"]}};

    this->accessor = j["accessor"];
    std::cout << "Loaded accessor: " << this->accessor << ", j: " << j << "\n";
}

} // namespace event_info