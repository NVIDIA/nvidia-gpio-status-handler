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

struct counterReset
{
    std::string type;
    std::string metadata;
};

struct Message
{
    std::string severity;
    std::string resolution;
};

struct redfish
{
    std::string messageId;
    Message message;
};

/** @class EventNode
 *  @brief Object to represent HMC events and store contents
 *         from event_info json profile
 */
class EventNode : public object::Object
{

  public:
    EventNode(const std::string& name = __PRETTY_FUNCTION__) :
        object::Object(name)
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

    /** @brief What triggered event **/
    std::string eventTrigger;

    /** @brief Accesssor info **/
    data_accessor::DataAccessor accessor;

    /** @brief Event count map: maps device name to count **/
    std::map<std::string, int> count;

    /** @brief Count that will trigger event **/
    int triggerCount;

    /** @brief Struct containing type and metadata **/
    data_accessor::DataAccessor counterReset;

    /** @brief Redfish fields struct **/
    redfish messageRegistry;

    /** @brief List of the event's telemetries **/
    std::vector<std::string> telemetries;

    /** @brief Action HMC should take **/
    std::string action;

    /** @brief Particular device name .. i.e. GPU0 **/
    std::string device;
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
