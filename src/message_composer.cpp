
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
                    event.messageRegistry.message.severity;
    method.append(severity);

    auto messageArgs = createMessageArgs(event);
    auto telemetries = collectDiagData(event);

    std::string oocDevice;
    if (dat.count(event.device) > 0)
    {
        oocDevice = dat.at(event.device).healthStatus.originOfCondition;
    }
    else
    {
        std::cerr << "Device does not exist in DAT: " << event.device
                  << std::endl;
        return false;
    }

    auto originOfCondition = getDeviceDBusPath(oocDevice);

    // TODO: auto telemetries = Compression(telemetries);

#ifdef ENABLE_LOGS
    std::cerr << "OOC device for " << event.device << " is " << originOfCondition << " !!!!\n";
#endif

    method.append(std::array<std::pair<std::string, std::string>, 6>(
        {{{"xyz.openbmc_project.Logging.Entry.Resolution",
           event.messageRegistry.message.resolution},
          {"REDFISH_MESSAGE_ID", event.messageRegistry.messageId},
          {"DEVICE_EVENT_DATA", telemetries},
          {"namespace", event.device},
          {"REDFISH_MESSAGE_ARGS", messageArgs},
          {"REDFISH_ORIGIN_OF_CONDITION", originOfCondition}}}));

    try
    {
        auto reply = bus.call(method);
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
