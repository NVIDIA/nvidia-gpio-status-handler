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
        std::cout << "new device type: " << deviceType << "\r\n";

        for (const auto& event : el.value())
        {
            std::cout << "\tcreate event (" << event["event"] << ").\n";

            event_info::EventNode eventNode(event["event"]);

            std::cout << "\tload event (" << event["event"] << ").\n";
            eventNode.loadFrom(event);

            std::cout << "\tpush event (" << event["event"] << ").\n";

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
            event.print();
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

static void print_accessor([[maybe_unused]] const data_accessor::DataAccessor& acc) {
    /* todo */
    return;
}

static void print_node(const EventNode& n)
{
    std::cout << "\tDumping event     " << n.event << "\n";

    std::cout << "\t\tdeviceType      " << n.deviceType << "\n";
    std::cout << "\t\teventTrigger    " << n.eventTrigger << "\n";
    std::cout << "\t\taccessor        " << "todo" << "\n";
    print_accessor(n.accessor);
    std::cout << "\t\tcount(map)      " << n.count.size() << "\n";
    for(auto& p : n.count){
        std::cout << "\t\t\t[" << p.first << "] = " << p.second << "\n";
    }

    std::cout << "\t\ttriggerCount    " << n.triggerCount << "\n";
    std::cout << "\t\tcounterReset    " << "todo" << "\n";
    print_accessor(n.counterReset);

    std::cout << "\t\tmessageRegistry " << n.messageRegistry.messageId << "\n";
    std::cout << "\t\t\t" << n.messageRegistry.message.severity << "\n";
    std::cout << "\t\t\t" << n.messageRegistry.message.resolution << "\n";

    std::cout << "\t\ttelemetries     " << n.telemetries.size() <<"\n";
    for(auto& v : n.telemetries){
        std::cout << "\t\t\t" << v << "\n";
    }

    std::cout << "\t\taction          " << n.action << "\n";
    std::cout << "\t\tdevice          " << n.device << "\n";
}

void EventNode::print(const EventNode& n) const
{
    print_node(n);
}

void EventNode::print(void) const
{
    print_node(*this);
}

} // namespace event_info
