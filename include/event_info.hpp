/*
 *
 */

#pragma once

#include "data_accessor.hpp"
#include "device_id.hpp"
#include "json_proc.hpp"
#include "object.hpp"
#include "device_id.hpp"

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
    // marcinw:TODO: to remove
    std::vector<MessageArg> messageArgs;
    nlohmann::json messageArgsJsonPattern;

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

    /** @brief Dumps current object class content to stdout
     */
    void print(void) const;

  private:
    /** @brief The copy of the original json event node **/
    nlohmann::json configEventNode;

    /** @brief A sequence of numbers identifying a device this event occured on
     *
     *  Example, Object path: /xyz/openbmc_project/NVSwitch_2/Ports/NVLink_23
     *  Supposing the example above, it will contain (2,23)
     **/
    std::optional<device_id::PatternIndex> deviceIndexTuple;

    /** @brief Redfish fields struct **/
    redfish messageRegistry;

    /** @brief Type of device **/
    std::vector<std::string> deviceTypes;

  public:
    /** @brief Name of the event **/
    std::string event;

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

    /**
     * @brief Setter for @c deviceIndexTuple field
     */
    void setDeviceIndexTuple(const device_id::PatternIndex& deviceIndexTuple);

    // No getter provided for 'deviceIndexTuple'. This is an abstract identifier
    // used for inner mechanics of determining the source of an event. If a user
    // of 'EventNode' requires this information from this class then he's
    // probably trying to implement something that should be placed here.

    /**
     * @brief True if @c setDeviceIndexTuple has been called on this object,
     * false otherwise
     */
    bool isEventNodeEvaluated() const;

    /**
     * @brief Return the severity of the event
     *
     * E.g. "Critical", "Warning", "OK"
     */
    std::string getSeverity() const;

    /**
     * @brief Return the resolution of the problem specified by this event
     *
     * E.g "Reset the link. If problem persists, isolate the server for RMA
     * evaluation."
     */
    std::string getResolution() const;

    /**
     * @brief Return a code specifying the message format string describing the
     * event in redfish
     *
     * E.g. "ResourceEvent.1.0.ResourceErrorsDetected"
     */
    std::string getMessageId() const;

    /**
     * @brief Return the arguments provided to the format string specified by @c
     * getMessageId concatenated with comma
     *
     * This is the format expected by bmcweb.
     *
     * E.g. "NVSwitch_1 PCIe, Abnormal Speed Change", "GPU_SXM_4 Memory,
     * Row-Remapping Pending"
     */
    std::string getStringMessageArgs();

    /**
     * @brief Call @c setDeviceTypes(js) wrapping it in a sensible error message
     */
    void readDeviceTypes(const json& js, const std::string& eventName);

    /**
     * @brief Set the @c deviceTypes field directly from a corresponding
     * attribute of the event node in the event info json config file
     */
    void setDeviceTypes(const json& js);

    /**
     * @brief Setter for @c deviceTypes field
     */
    void setDeviceTypes(std::vector<std::string>&& values);

    /**
     * @brief Return a string representation of the devices set this event is
     * associated with
     *
     * If @c deviceTypes has one element return the content of this element. If
     * it has more elements concatenate them with '/' and return as a single
     * string. The situation of @c deviceTypes having no elements will not
     * happen because @c setDeviceTypes will reject it.
     *
     * E.g. return "GPU_SXM_[1-8]" for @c deviceTypes being
     *
     * @code
     * ["GPU_SXM_[1-8]"]
     * @endcode
     *
     * Return "NVSwitch_[0|0-3]/NVLink_[1|0-39]" for @c deviceTypes being
     *
     * @code
     * ["NVSwitch_[0|0-3]", "NVLink_[1|0-39]"]
     * @endcode
     */
    std::string getStrigifiedDeviceType() const;

    /**
     * @brief Return the first element of @c deviceTypes
     */
    std::string getMainDeviceType() const;

    /**
     * @brief Return whether the DataAccessor is interesting
     * (matches our D-Bus object/interface/property)
    */
    static bool getIsAccessorInteresting(EventNode& event,
        data_accessor::DataAccessor& otherAccessor);
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
