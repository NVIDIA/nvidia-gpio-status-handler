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

struct eventCounterReset
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
    Message messageStruct;
};

/** @class EventNode
 *  @brief Object to represent HMC events and store contents
 *         from event_info json profile
 */
class EventNode : public object::Object
{

  public:
    EventNode(const std::string& name) : object::Object(name)
    {}

  public:
    /** @brief Name of the event **/
    std::string event;

    /** @brief Type of device **/
    std::string deviceType;

    /** @brief What triggered event **/
    std::string eventTrigger;

    /** @brief Accesssor info **/
    data_accessor::DataAccessor accessorStruct;

    /** @brief Count that will trigger event **/
    int triggerCount;

    /** @brief Struct containing type and metadata **/
    eventCounterReset eventCounterResetStruct;

    /** @brief Redfish fields struct **/
    redfish redfishStruct;

    /** @brief List of the event's telemetries **/
    std::vector<std::string> telemetries;

    /** @brief Action HMC should take **/
    std::string action;

    /** @brief Particular device name .. i.e. GPU0 **/
    std::string device;

    /** @brief Event count **/
    int eventCount;

    /** @brief Load class contents from JSON profile
     *
     * @param[in]  j  - json object
     *
     */
    void loadFrom(const json& j);
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