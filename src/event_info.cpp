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

    std::ifstream i(file);
    json j;
    i >> j;

    for (const auto& el : j.items())
    {
        auto deviceType = el.key();
        std::vector<event_info::EventNode> v;

        for (const auto& event : el.value())
        {

            event_info::EventNode eventNode(event["event"]);

            eventNode.loadFrom(event);

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

    eventCounterReset eventCounterResetStruct = {
        j["event_counter_reset"]["type"].get<std::string>(),
        j["event_counter_reset"]["metadata"].get<std::string>()};

    this->eventCounterResetStruct = eventCounterResetStruct;

    Message messageStruct = {j["severity"], j["resolution"]};

    redfish redfishStruct = {j["redfish"]["message_id"].get<std::string>(),
                             messageStruct};

    this->redfishStruct = redfishStruct;
}

} // namespace event_info