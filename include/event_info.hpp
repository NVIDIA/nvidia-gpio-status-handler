/*
 *
 */

#pragma once

#include "data_accessor.hpp"
#include "object.hpp"

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace event_info
{
class EventNode;

struct counterReset
{
    std::string type;
    std::string metadata;
};

struct Message
{
    std::string severity;
    std::string resolution;

  public:
    /**
     * @brief Print this object to the output stream @c os (e.g. std::cout,
     * std::cerr, std::stringstream) with every line prefixed with @c indent.
     *
     * For the use with logging framework use the following construct:
     *
     * @code
     *   std::stringstream ss;
     *   obj.print(ss, indent);
     *   log_dbg("%s", ss.str().c_str());
     * @endcode
     */
    template <class CharT>
    void print(std::basic_ostream<CharT>& os = std::cout,
               std::string indent = std::string("")) const
    {
        os << indent << "severity:"
           << "\t" << severity << std::endl;
        os << indent << "resolution:"
           << "\t" << resolution << std::endl;
    }
};

struct MessageArgPattern
{
    std::string pattern;

  public:
    /**
     * @brief Get the number of placeholders (substrings of the form '{...}') in
     * the @c pattern
     */
    unsigned placeholdersCount() const;

    /**
     * @brief Get a string with all placeholders (substrings of the form
     * '{...}') replaced with the corresponding values in @c values
     *
     * For example, given object @c x with @c x.pattern being "name: {TheName},
     * age: {TheAge}" the @c substPlaceholders({"Karen", "42"}) returns "name:
     * Karen, age: 42".
     *
     * The @c pattern field is not modified.
     *
     * @param[in] values It's assumed the length of the vector is equal to the
     * value @c placeholdersCount() would return.
     */
    std::string substPlaceholders(const std::vector<std::string>& values) const;

    /**
     * @brief Print this object to the output stream @c os (e.g. std::cout,
     * std::cerr, std::stringstream) with every line prefixed with @c indent.
     *
     * For the use with logging framework use the following construct:
     *
     * @code
     *   std::stringstream ss;
     *   obj.print(ss, indent);
     *   log_dbg("%s", ss.str().c_str());
     * @endcode
     */
    template <class CharT>
    void print(std::basic_ostream<CharT>& os = std::cout,
               std::string indent = std::string("")) const
    {
        os << indent << pattern << std::endl;
    }
};

struct MessageArg
{
    MessageArgPattern pattern;
    std::vector<data_accessor::DataAccessor> parameters;

  public:
    /**
     * @brief Print this object to the output stream @c os (e.g. std::cout,
     * std::cerr, std::stringstream) with every line prefixed with @c indent.
     *
     * For the use with logging framework use the following construct:
     *
     * @code
     *   std::stringstream ss;
     *   obj.print(ss, indent);
     *   log_dbg("%s", ss.str().c_str());
     * @endcode
     */
    template <class CharT>
    void print(std::basic_ostream<CharT>& os = std::cout,
               std::string indent = std::string("")) const
    {
        os << indent << "pattern:" << std::endl;
        pattern.print(os, indent + "\t");
        os << indent << "parameters:" << std::endl;
        util::print(parameters, os, indent + "\t");
    }

    /**
     * @brief Get a string with values from the accessors inserted into '{...}'
     * placeholders in @c pattern
     *
     * Call the accessors in sequence and insert the values they return in the
     * placeholders in `pattern'. For instance, assuming this object represents
     * the first message arg ("{GPUId} Temperature") of the json snippet
     *
     * ,----
     * | "message_args": [
     * |   "{GPUId} Temperature",
     * |   "{UpperFatal Threshold}"
     * | ],
     * | "parameters": [
     * |   {
     * |     "type": "DIRECT",
     * |     "field": "CurrentDeviceName"
     * |   },
     * |   {
     * |     "type": "...",
     * |     "interface": "...",
     * |     "object": "...",
     * |     "property": "..."
     * |   }
     * | ]
     * `----
     *
     * and the event occured on "GPU3" the string "GPU3 Temperature" will be
     * returned.
     *
     * @param[in] event Information needed to compose the message args about the
     * event. The object passed can (and most probably should) be the one @c
     * *this is contained in as the @c messageRegistry.messageArgs[...] field.
     */
    std::string getStringMessageArg(const EventNode& event);
};

struct redfish
{
    std::string messageId;
    Message message;
    std::vector<MessageArg> messageArgs;

  public:
    /**
     * @brief Print this object to the output stream @c os (e.g. std::cout,
     * std::cerr, std::stringstream) with every line prefixed with @c indent.
     *
     * For the use with logging framework use the following construct:
     *
     * @code
     *   std::stringstream ss;
     *   obj.print(ss, indent);
     *   log_dbg("%s", ss.str().c_str());
     * @endcode
     */
    template <class CharT>
    void print(std::basic_ostream<CharT>& os = std::cout,
               std::string indent = std::string("")) const
    {
        os << indent << "messageId:"
           << "\t" << this->messageId << std::endl;
        os << indent << "message:" << std::endl;
        message.print(os, indent + "\t");
        os << indent << "messageArgs:" << std::endl;
        util::print(messageArgs, os, indent + "\t");
    }

    /**
     * @brief Evaluate the dynamic message arg and encode them into a single
     * string
     *
     * Concatenate the results of evaluating each dynamic message arg with a
     * comma. For instance, assuming this object represents the json snippet
     *
     * @code
     * "message_args": {
     *   "patterns": [
     *     "{GPUId} Temperature",
     *     "{UpperFatal Threshold}"
     *   ],
     *   "parameters": [
     *     {
     *       "type": "DIRECT",
     *       "field": "CurrentDeviceName"
     *     },
     *     {
     *       "type": "DBUS",
     *       "interface": "...",
     *       "object": "...",
     *       "property": "..."
     *     }
     *   ]
     * }
     *
     * @endcode
     *
     * and
     * 1. the event occured on "GPU3" device
     * 2. dbus call returned "120" string
     *
     * the string "GPU3 Temperature, 120" will be returned.
     *
     * @param[in] event Information needed to compose the message args about the
     * event. The object passed can (and most probably should) be the one @c
     * *this is contained in as the @c messageRegistry field.
     */
    std::string getStringMessageArgsDynamic(const EventNode& event);

    /**
     * @brief Compose REDFISH_MESSAGE_ARGS from event
     *
     * Behave like @c getStringMessageArgsDynamic except ignore the actual @c
     * messageArgs value and act as if it corresponded to the following
     * configuration:
     *
     * @code
     * "message_args": {
     *   "message_args": [
     *     "{}",
     *     EVENT_NAME
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
     * where EVENT_NAME is simply the value of @c event.event.
     *
     * This function is provided for backward compatibility.
     *
     * @param event
     * @return std::string&
     */
    std::string getStringMessageArgsStatic(const EventNode& event);

    /**
     * @brief Fall back to @c getStringMessageArgsDynamic or @c
     * getStringMessageArgsStatic depending on @c messageArgs size.
     */
    std::string getStringMessageArgs(const EventNode& event);
};

/** @class EventNode
 *  @brief Object to represent HMC events and store contents
 *         from event_info json profile
 */
class EventNode : public object::Object
{

  public:
    EventNode(const std::string& name = __PRETTY_FUNCTION__) :
        object::Object(name), triggerCount(0), valueAsCount(false)
    {}

    EventNode(const EventNode& r) = default;

  public:
    /** @brief Load class contents from JSON profile
     *
     * @param[in]  j  - json object
     *
     */
    void loadFrom(const json& j);

    /** @brief Dumps class content to stdout
     *
     * @param[in]  n  - event node object to dump
     *
     */
    void print(const EventNode& n) const;

    /** @brief Dumps current object class content to stdout
     */
    void print(void) const;

  public:
    /** @brief Name of the event **/
    std::string event;

    /** @brief Type of device **/
    std::string deviceType;

    /** @brief Type of device subtype **/
    std::string subType;

    /** @brief What triggered event **/
    std::string eventTrigger;

    /** @brief Accesssor info **/
    data_accessor::DataAccessor accessor;

     /** @brief Accesssor info **/
    data_accessor::DataAccessor recovery_accessor;

    /** @brief Trigger accessor info **/
    data_accessor::DataAccessor trigger;

    /** @brief Event count map: maps device name to count **/
    std::map<std::string, int> count;

    /** @brief Count that will trigger event **/
    int triggerCount;

    /** @brief Struct containing type and metadata **/
    data_accessor::DataAccessor counterReset;

    /** @brief Redfish fields struct **/
    redfish messageRegistry;

    /** @brief List of the event's telemetries **/
    std::vector<data_accessor::DataAccessor> telemetries;

    /** @brief Action HMC should take **/
    std::string action;

    /** @brief Particular device name .. i.e. GPU0 **/
    std::string device;

    /** @brief Flag to indicate if we should use event value as trigger count
     * **/
    bool valueAsCount;

    /** @brief Report of selftest **/
    nlohmann::ordered_json selftestReport;
};

using EventMap = std::map<std::string, std::vector<event_info::EventNode>>;

/** @brief Load class contents from JSON profile
 *
 * Wrapper method for loadFrom
 *
 * @param[in]  eventMap
 * @param[in]  file
 *
 */
void loadFromFile(EventMap& eventMap, const std::string& file);

/** @brief Prints out memory map to verify field population
 *
 * @param[in]  eventMap
 *
 */
void printMap(const EventMap& eventMap);

} // namespace event_info
