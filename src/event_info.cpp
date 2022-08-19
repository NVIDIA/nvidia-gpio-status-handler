/*
 *
 */

#include "event_info.hpp"

#include "util.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace event_info
{

std::string MessageArg::getStringMessageArg(const EventNode& event)
{
    std::vector<std::string> values;
    for (auto& accessor : parameters)
    {
        values.push_back(accessor.read(event));
    }
    return pattern.substPlaceholders(values);
}

std::string redfish::getStringMessageArgs(const EventNode& event)
{
    std::stringstream ss;
    std::string delimiter = ", ";
    for (unsigned i = 0u; i < messageArgs.size(); ++i)
    {
        if (i != 0)
        {
            ss << delimiter;
        }
        ss << messageArgs.at(i).getStringMessageArg(event);
    }
    return ss.str();
}

unsigned MessageArgPattern::placeholdersCount() const
{
    static const std::regex reg("^[^\\{]*\\{[^\\}]*\\}");
    std::smatch match;
    std::string str = pattern;
    unsigned matchesCount = 0;
    while (std::regex_search(str, match, reg))
    {
        str = match.suffix();
        matchesCount++;
    }
    return matchesCount;
}

std::string MessageArgPattern::substPlaceholders(
    const std::vector<std::string>& values) const
{
    static const std::regex matchReg("^[^\\{]*\\{[^\\}]*\\}");
    static const std::regex substReg("\\{[^\\}]*\\}");
    std::smatch matchObj;
    std::string str = pattern;
    std::string result;

    for (const auto& elem : values)
    {
        auto matches = std::regex_search(str, matchObj, matchReg);
        assert(matches);
        std::string matchedStr = matchObj.str(0);
        str = matchObj.suffix();
        result += std::regex_replace(matchedStr, substReg, elem);
    }
    result += str;
    return result;
}

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
        logs_dbg("%s", ss.str().c_str());

        for (const auto& event : el.value())
        {

            ss.str(std::string()); // Clearing the stream first
            ss << "\tcreate event (" << event["event"] << ").\n";
            logs_dbg("%s", ss.str().c_str());

            event_info::EventNode eventNode(event["event"]);

            ss.str(std::string()); // Clearing the stream first
            ss << "\tload event (" << event["event"] << ").\n";
            logs_dbg("%s", ss.str().c_str());
            eventNode.loadFrom(event);

            ss.str(std::string()); // Clearing the stream first
            ss << "\tpush event (" << event["event"] << ").\n";
            logs_dbg("%s", ss.str().c_str());

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

/**
 * @brief Read "patterns" property of the "redfish.message_args"
 * entry in event info node of the json config file.
 *
 * Auxiliary function to the @c EventNode::loadFrom(const json& j)
 *
 * Given the @c messageArgsJson parameter representing an object of the form,
 * eg:
 *
 * @code
 * {
 *   "patterns": [
 *     "{GPUId} Memory",
 *     "Uncontained ECC Error"
 *   ],
 *   "parameters": [
 *     {
 *       "type": "DIRECT",
 *       "field": "CurrentDeviceName"
 *     }
 *   ]
 * }
 * @endcode
 *
 * return a vector of elements
 *
 * @code
 * [0]: "{GPUId} Memory"
 * [1]: "Uncontained ECC Error"
 * @endcode
 */
std::vector<MessageArgPattern>
    loadMessageArgsPatterns(const json& messageArgsJson)
{
    assert(messageArgsJson.contains("patterns"));
    std::vector<MessageArgPattern> result;
    for (const json& elem : messageArgsJson["patterns"])
    {
        assert(elem.is_string());
        result.push_back(MessageArgPattern{elem.get<std::string>()});
    }
    return result;
}

/**
 * @brief Parse the subsequence of "parameters" attribute of the
 * "redfish.message_args" entry in event info node of the json config file.
 *
 * Auxiliary function to the @c EventNode::loadFrom(const json& j)
 *
 * Given the @c accessorsJson parameter representing an array of the form,
 * eg:
 *
 * @code
 * [
 *   {
 *     "type": "DIRECT",
 *     "field": "CurrentDeviceName"
 *   },
 *   {
 *     "type": "CONSTANT",
 *     "value": "plug"
 *   }
 * ]
 * @endcode
 *
 * and the @c fromIncl, @c toExcl paraemeters being 0, 1,
 * resepctively, return a vector of one object representing the
 *
 * @code
 * {
 *   "type": "DIRECT",
 *   "field": "CurrentDeviceName"
 * }
 * @endcode
 *
 * accessor definition.
 *
 * @param[in] accessorsJson The json array object representing the "parameters"
 * attribute of the "redfish.message_args" entry in event info node of the json
 * config file.
 *
 * @param[in] fromIncl The index in the json array @c accessorsJson from which
 * to start constructing @c DataAccessor objects, inclusive
 *
 * @param[in] toExcl The index in the json array @c accessorsJson to which end
 * constructing @c DataAccessor objects, exclusive
 */
std::vector<data_accessor::DataAccessor>
    loadMessageArgsAccessors(const json& accessorsJson, unsigned fromIncl,
                             unsigned toExcl)
{
    std::vector<data_accessor::DataAccessor> result;
    for (unsigned i = fromIncl; i < toExcl; ++i)
    {
        result.push_back(data_accessor::DataAccessor(accessorsJson[i]));
    }
    return result;
}

/**
 * @brief Parse the "message_args" property of the "redfish" entry in the event
 * info node of the 'event_info.json' file into a vector of objects.
 *
 * Auxiliary function to the @c EventNode::loadFrom(const json& j)
 *
 * The number of elements in the result correspond to the number of elements in
 * the "patterns" property. The "parameters" array is split into chunks and
 * coupled with the objects holding the pattern associated with it. For example,
 * given the "redfish" entry like
 *
 * @code
 * "redfish": {
 *   "message_id": "ResourceEvent.1.0.ResourceErrorThresholdExceeded",
 *   "message_args": {
 *     "patterns": [
 *       "{NVSwitchId} Temperature",
 *       "{UpperCritical Threshold}"
 *     ],
 *     "parameters": [
 *       {
 *         "type": "DIRECT",
 *         "field": "CurrentDeviceName"
 *       },
 *       {
 *         "type": "CONSTANT",
 *         "value": "plug"
 *       }
 *     ]
 *   }
 * }
 * @endcode
 *
 * the resulting vector will contain two @c MessageArg objects,
 * which can be represented as
 *
 * @code
 * [0]: ("{GPUId} Memory", { "type": "DIRECT","field": "CurrentDeviceName" })
 * [1]: ("Uncontained ECC Error", { "type": "CONSTANT","value": "plug" })
 * @endcode
 *
 * Including the literal characters '{' and '}' can be accomplished by using the
 * "CONSTANT"-type accessor containing those characters. For example, to obtain
 * the message arg "{x__x}" use this construct:
 *
 * @code
 *     "patterns": [
 *       "{}",
 *     ],
 *     "parameters": [
 *       {
 *         "type": "CONSTANT",
 *         "value": "{x__x}"
 *       }
 *     ]
 * @endcode
 *
 * @param[in] messageArgsJson A json object representing the value of
 * "redfish.message_args" entry in the even info node
 */
std::vector<event_info::MessageArg> loadMessageArgs(const json& messageArgsJson)
{
    std::vector<MessageArgPattern> patterns =
        loadMessageArgsPatterns(messageArgsJson);

    json accessorsJson;
    if (messageArgsJson.contains("parameters"))
    {
        accessorsJson = messageArgsJson["parameters"];
    }
    else
    {
        // assume the empty "parameters" array was passed
        accessorsJson = json::array();
    }
    assert(accessorsJson.is_array());

    unsigned totalPlaceholders =
        std::accumulate(patterns.begin(), patterns.end(), 0u,
                        [](const unsigned& res, const MessageArgPattern& p) {
                            return res + p.placeholdersCount();
                        });
    if (totalPlaceholders != accessorsJson.size())
    {
        std::stringstream ss;
        ss << "The total number of placeholders in \"patterns\" property (= "
           << totalPlaceholders
           << ") is different from the number of elements in the \"parameters\" property (= "
           << accessorsJson.size() << ") for the \"message_args\" value '"
           << messageArgsJson << "'" << std::endl;
        logs_err("%s", ss.str().c_str());
        throw std::runtime_error(ss.str().c_str());
    }

    std::vector<event_info::MessageArg> result;
    unsigned placeholdersRead = 0u;
    for (const auto& pattern : patterns)
    {
        unsigned count = pattern.placeholdersCount();
        result.push_back(MessageArg{
            pattern, loadMessageArgsAccessors(accessorsJson, placeholdersRead,
                                              placeholdersRead + count)});
        placeholdersRead += count;
    }

    return result;
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
                             {j["severity"], j["resolution"]},
                             loadMessageArgs(j["redfish"]["message_args"])};

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

    // ss << "\t\tmessageRegistry " << n.messageRegistry;

    // ss << "\t\tmessageRegistry " << n.messageRegistry.messageId << "\n";

    ss << "\t\tredfish:" << std::endl;
    n.messageRegistry.print(ss, "\t\t\t");

    ss << "\t\t\t" << n.messageRegistry.message.severity << "\n";
    ss << "\t\t\t" << n.messageRegistry.message.resolution << "\n";

    ss << "\t\ttelemetries     " << n.telemetries.size() << "\n";
    for (auto& v : n.telemetries)
    {
        ss << "\t\t\t" << v << "\n";
    }

    ss << "\t\taction          " << n.action << "\n";
    ss << "\t\tdevice          " << n.device << "\n";

    // Should be compatible with 'printMap'
    std::cerr << ss.str();
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
