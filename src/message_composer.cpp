
/*
 *
 */

#include "message_composer.hpp"

#include "aml.hpp"
#include "event_handler.hpp"
#include "event_info.hpp"

#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/exception.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <utility>
#include <vector>

namespace message_composer
{

using phosphor::logging::entry;
using phosphor::logging::level;
using phosphor::logging::log;

MessageComposer::~MessageComposer()
{}

bool MessageComposer::createLog(event_info::EventNode& event)
{
    auto bus = sdbusplus::bus::new_default_system();
    auto method = bus.new_method_call(
        "xyz.openbmc_project.Logging", "/xyz/openbmc_project/logging",
        "xyz.openbmc_project.Logging.Create", "Create");
    method.append(event.event);
    auto severity = std::string("xyz.openbmc_project.Logging.Entry.Level.") +
                    event.redfishStruct.messageStruct.severity;
    method.append(severity);

    auto telemetries = std::accumulate(
        event.telemetries.begin(), event.telemetries.end(), std::string(""));

    method.append(std::array<std::pair<std::string, std::string>, 3>(
        {{{"xyz.openbmc_project.Logging.Entry.Resolution",
           event.redfishStruct.messageStruct.resolution},
          {"REDFISH_MESSAGE_ID", event.redfishStruct.messageId},
          {"DEVICE_EVENT_DATA", telemetries}}}));
    try
    {
        auto reply = bus.call(method);
        std::vector<
            std::tuple<uint32_t, std::string, sdbusplus::message::object_path>>
            users;
        reply.read(users);
        for (auto& user : users)
        {
            std::cerr << std::get<std::string>(user) << "\n";
        }
        return true;
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        std::cerr << "ERROR CREATING LOG " << e.what() << "\n";
        log<level::ERR>("Failed to create log for event",
                        entry("SDBUSERR=%s", e.what()));
        return false;
    }
}

} // namespace message_composer
