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
    std::stringstream ss;
    ss << "loadFromFile func (" << file << ").";
    logs_dbg("%s\n", ss.str().c_str());
    std::ifstream i(file);
    json j;
    i >> j;

    for (const auto& el : j.items())
    {
        auto deviceType = el.key();
        std::vector<event_info::EventNode> v = {};
        ss.str(std::string()); // Clearing the stream first
        ss << "new device type: " << deviceType << "\r\n";
        logs_dbg("%s", ss.str().c_str()) ;

        for (const auto& event : el.value())
        {
            
            ss.str(std::string()); // Clearing the stream first
            ss << "\tcreate event (" << event["event"] << ").\n";
            logs_dbg("%s", ss.str().c_str()) ;

            event_info::EventNode eventNode(event["event"]);

            ss.str(std::string()); // Clearing the stream first
            ss << "\tload event (" << event["event"] << ").\n";
            logs_dbg("%s", ss.str().c_str()) ;
            eventNode.loadFrom(event);

            ss.str(std::string()); // Clearing the stream first
            ss << "\tpush event (" << event["event"] << ").\n";
            logs_dbg("%s", ss.str().c_str()) ;

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
    // this->eventTrigger = j["event_trigger"];

    this->trigger = j["event_trigger"];

    for (auto& entry : j["telemetries"])
    {
        this->telemetries.push_back((data_accessor::DataAccessor)entry);
    }
    // this->telemetries = j["telemetries"].get<std::vector<std::string>>();
    this->action = j["action"];
    this->device = "";

    this->counterReset = j["event_counter_reset"];

    this->messageRegistry = {j["redfish"]["message_id"].get<std::string>(),
                             {j["severity"], j["resolution"]}};

    this->accessor = j["accessor"];
    
    std::stringstream ss;
    ss << "Loaded accessor: " << this->accessor << ", j: " << j;
    log_dbg("%s\n", ss.str().c_str());

    this->valueAsCount =
        j.contains("value_as_count") ? j["value_as_count"].get<bool>() : false;
}

// Not used anymore - may get rid of it later
// static void
//     print_accessor([[maybe_unused]] const data_accessor::DataAccessor& acc)
// {
//     /* todo */
//     return;
// }


static void print_node(const EventNode& n)
{
    std::stringstream ss;
    ss << n.accessor << "\n";
    ss << "\tDumping event     " << n.event << "\n";

    ss << "\t\tdeviceType      " << n.deviceType << "\n";
    ss << "\t\teventTrigger    " << n.eventTrigger << "\n";
    ss << "\t\taccessor        "
              << "todo"
              << "\n";
    ss << n.accessor << "\n";

    ss << "\t\tcount(map)      " << n.count.size() << "\n";
    for (auto& p : n.count)
    {
        ss << "\t\t\t[" << p.first << "] = " << p.second << "\n";
    }

    ss << "\t\ttriggerCount    " << n.triggerCount << "\n";
    ss << "\t\tcounterReset    "
              << "todo"
              << "\n";
    ss << n.counterReset << "\n";

    ss << "\t\tmessageRegistry " << n.messageRegistry.messageId << "\n";
    ss << "\t\t\t" << n.messageRegistry.message.severity << "\n";
    ss << "\t\t\t" << n.messageRegistry.message.resolution << "\n";

    ss << "\t\ttelemetries     " << n.telemetries.size() << "\n";
    for (auto& v : n.telemetries)
    {
        ss << "\t\t\t" << v << "\n";
    }

    ss << "\t\taction          " << n.action << "\n";
    ss << "\t\tdevice          " << n.device << "\n";
    logs_dbg("%s", ss.str().c_str());
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
