/*
 *
 */

#include "event_info.hpp"

#include "util.hpp"

#include <boost/algorithm/string/split.hpp>
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

std::string redfish::getStringMessageArgsDynamic(const EventNode& event)
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

std::string redfish::getStringMessageArgsStatic(const EventNode& event)
{
    std::vector<std::string> args;
    args.push_back(event.device);
    args.push_back(event.event);
    std::string msg = "";
    for (auto it = args.begin(); it != std::prev(args.end()); it++)
    {
        msg += *it + ", ";
    }
    msg += args.back();
    return msg;
}

std::string redfish::getStringMessageArgs(const EventNode& event)
{
    if (messageArgs.size() > 0)
    {
        return getStringMessageArgsDynamic(event);
    }
    else
    {
        return getStringMessageArgsStatic(event);
    }
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

void EventNode::readDeviceTypes(const json& js, const std::string& eventName)
{
    try
    {
        this->setDeviceTypes(js);
    }
    catch (const std::exception& e)
    {
        std::stringstream ss;
        ss << "Error reading \"device_type\" property in event entry '"
           << eventName << "': " << e.what();
        shortlog_err(<< ss.str());
        throw std::runtime_error(ss.str());
    }
}

void EventNode::loadFrom(const json& j)
{
    this->configEventNode = j;

    this->event = j["event"];
    readDeviceTypes(j["device_type"], this->event);
    this->triggerCount = j["trigger_count"].get<int>();
    // this->eventTrigger = j["event_trigger"];

    this->trigger = j["event_trigger"];
    this->subType = j.value("sub_type", "");

    for (auto& entry : j["telemetries"])
    {
        this->telemetries.push_back((data_accessor::DataAccessor)entry);
    }
    this->action = j["action"];
    this->device = "";

    this->counterReset = j["event_counter_reset"];

    // std::vector<event_info::MessageArg> messageArgs;
    // if (j["redfish"].contains("message_args"))
    // {
    //     messageArgs = loadMessageArgs(j["redfish"]["message_args"]);
    // }
    // // otherwise leave `messageArgs' empty

    this->messageRegistry = {j["redfish"]["message_id"].get<std::string>(),
                             {j["severity"], j["resolution"]},
                             {},
                             j["redfish"].contains("message_args")
                                 ? j["redfish"]["message_args"]
                                 : nlohmann::json()};

    this->accessor = j["accessor"];

    std::stringstream ss;
    ss << "Loaded accessor: " << this->accessor << ", j: " << j;
    log_dbg("%s\n", ss.str().c_str());

    if (j.contains("recovery"))
    {
        this->recovery_accessor = j["recovery"];
        std::stringstream ss2;
        ss2 << "Loaded accessor: " << this->recovery_accessor << ", j: " << j;
        log_dbg("%s\n", ss2.str().c_str());
    }

    this->valueAsCount =
        j.contains("value_as_count") ? j["value_as_count"].get<bool>() : false;
}

void EventNode::print() const
{
    std::stringstream ss;
    ss << accessor << "\n";
    ss << "\tDumping event     " << event << "\n";

    ss << "\t\tdeviceType      " << getStrigifiedDeviceType() << "\n";
    ss << "\t\teventTrigger    " << eventTrigger << "\n";
    ss << "\t\taccessor        "
       << "todo"
       << "\n";
    ss << accessor << "\n";

    ss << "\t\tcount(map)      " << count.size() << "\n";
    for (auto& p : count)
    {
        ss << "\t\t\t[" << p.first << "] = " << p.second << "\n";
    }

    ss << "\t\ttriggerCount    " << triggerCount << "\n";
    ss << "\t\tcounterReset    "
       << "todo"
       << "\n";
    ss << counterReset << "\n";

    ss << "\t\tredfish:" << std::endl;
    messageRegistry.print(ss, "\t\t\t");

    ss << "\t\t\t" << messageRegistry.message.severity << "\n";
    ss << "\t\t\t" << messageRegistry.message.resolution << "\n";

    ss << "\t\ttelemetries     " << telemetries.size() << "\n";
    for (auto& v : telemetries)
    {
        ss << "\t\t\t" << v << "\n";
    }

    ss << "\t\taction          " << action << "\n";
    ss << "\t\tdevice          " << device << "\n";

    // Should be compatible with 'printMap'
    std::cerr << ss.str();
}

void EventNode::setDeviceIndexTuple(
    const device_id::PatternIndex& deviceIndexTuple)
{
    this->deviceIndexTuple = deviceIndexTuple;
}

bool EventNode::isEventNodeEvaluated() const
{
    return deviceIndexTuple.has_value();
}

std::string EventNode::getSeverity() const
{
    return messageRegistry.message.severity;
}

std::string EventNode::getResolution() const
{
    return messageRegistry.message.resolution;
}

std::string EventNode::getMessageId() const
{
    return messageRegistry.messageId;
}

std::string EventNode::getStringMessageArgs()
{
    if (isEventNodeEvaluated())
    {
        try
        {
            log_dbg("messageRegistry.messageArgsJsonPattern: %s\n",
                    messageRegistry.messageArgsJsonPattern.dump().c_str());
            if (messageRegistry.messageArgsJsonPattern.is_null())
            {
                messageRegistry.messageArgs.clear();
            }
            else
            {
                nlohmann::json messageArgsJsonEvaluation =
                    json_proc::JsonPattern(
                        messageRegistry.messageArgsJsonPattern)
                        .eval(*deviceIndexTuple);
                log_dbg("messageArgsJsonEvaluation: %s",
                        messageArgsJsonEvaluation.dump().c_str());
                messageRegistry.messageArgs =
                    loadMessageArgs(messageArgsJsonEvaluation);
            }
            return messageRegistry.getStringMessageArgs(*this);
        }
        catch (const std::exception& e)
        {
            shortlog_err(
                << "An error occured during \"message_args\" processing: "
                << e.what() << ". Returning empty message args string");
            return "";
        }
    }
    else
    {
        shortlog_err(
            << "Called 'getStringMessageArgs' on an unevaluated event node. "
            << "Returning empty message args string");
        return "";
    }
}

void EventNode::setDeviceTypes(const json& js)
{
    if (js.is_string())
    {
        std::vector<std::string> elements;
        boost::split(elements, js.get<std::string>(), boost::is_any_of("/"));
        setDeviceTypes(elements);
    }
    else if (js.is_array())
    {
        std::vector<std::string> result;
        for (const auto& elem : js)
        {
            if (elem.is_string())
            {
                result.push_back(elem.get<std::string>());
            }
            else // ! elem.is_string()
            {
                std::stringstream ss;
                ss << "Value '" << elem.dump() << "' expected to be a string";
                throw std::runtime_error(ss.str());
            }
        }
        setDeviceTypes(std::move(result));
    }
    else
    {
        std::stringstream ss;
        ss << "value '" << js.dump()
           << "' expected to be a string or an array of strings";
        throw std::runtime_error(ss.str());
    }
}

void EventNode::setDeviceTypes(std::vector<std::string>&& values)
{
    if (values.size() > 0)
    {
        this->deviceTypes = std::move(values);
    }
    else
    {
        std::stringstream ss;
        ss << "Device types array is expected to have at least 1 element. "
           << "Actual size: " << values.size();
        throw std::runtime_error(ss.str());
    }
}

std::string EventNode::getStrigifiedDeviceType() const
{
    return boost::algorithm::join(this->deviceTypes, "/");
}

std::string EventNode::getMainDeviceType() const
{
    return this->deviceTypes.at(0);
}

bool EventNode::getIsAccessorInteresting(EventNode& event,
    data_accessor::DataAccessor& otherAccessor)
{
    return event.accessor == otherAccessor ||
        (!event.trigger.isEmpty() && event.trigger == otherAccessor);
}

} // namespace event_info
