/*
 *
 */

#pragma once

#include "object.hpp"
#include "data_accessor.hpp"

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace event_info {

struct eventCounterReset {
  std::string type;
  std::string metadata;
};

struct Message
{
  std::string severity;
  std::string resolution;
};

struct redfish {
  std::string messageId;
  Message messageStruct;
};


class EventNode : public object::Object {

public:
    EventNode(const std::string& name) : object::Object(name){}
public:

  /* name of event */
  std::string event;

  /* type of device ... i.e. gpu */
  std::string deviceType;

  /* what triggered the event */
  std::string eventTrigger;

  /* accessor info */
  data_accessor::DataAccessor* accessorStruct;

  /* count that will trigger event */
  int triggerCount;

  /* struct for event counter reset data */
  eventCounterReset *eventCounterResetStruct;

  /* redfish struct to encapsulate messageid, severity, resolution */
  redfish *redfishStruct;

  /* list of this event's telemetries */
  std::vector<std::string> telemetries;

  /* action hmc should take */
  std::string action;

  void loadFrom(const json& j);
};

using EventMap = std::map<std::string, std::vector<event_info::EventNode>>;

/* populates memory structure with json profile file */
void loadFromFile(
      EventMap& eventMap,
      const std::string& file);

/* prints out memory structure to verify population went as expected */
void printMap(const EventMap& eventMap);


} // namespace event_info