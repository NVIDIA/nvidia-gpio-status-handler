/*
 *
 */

#pragma once

#include "dat_traverse.hpp"
#include <map>
#include <nlohmann/json.hpp>
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


class EventNode {
public:

  /* name of event */
  std::string event;

  /* type of device ... i.e. gpu */
  std::string deviceType;

  /* what triggered the event */
  std::string eventTrigger;

  /* accessor info */
  dat_traverse::accessor *accessorStruct;

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

  /* populates memory structure with json profile contents */
  static void populateMap(
      std::map<std::string, std::vector<event_info::EventNode>> &eventMap,
      const json& j);

  /* prints out memory structure to verify population went as expected */
  static void
  printMap(const std::map<std::string, std::vector<event_info::EventNode>>& eventMap);

  EventNode(const json& j);
};

} // namespace event_info