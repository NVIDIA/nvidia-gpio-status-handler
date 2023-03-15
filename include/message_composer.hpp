
/*
 *
 */

#pragma once

#include "aml.hpp"
#include "dat_traverse.hpp"
#include "data_accessor.hpp"
#include "event_handler.hpp"
#include "event_info.hpp"

#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/exception.hpp>

#include <string>

namespace message_composer
{

/**
 * @brief A class for composing event log entry and create it in
 * phosphor-logging.
 *
 */
class MessageComposer : public event_handler::EventHandler
{
  public:
    MessageComposer(std::map<std::string, dat_traverse::Device>& datMap,
                    const std::string& name = __PRETTY_FUNCTION__) :
        event_handler::EventHandler(name),
        dat(datMap)

    {}
    ~MessageComposer();

  public:
    /**
     * @brief Turn event into phosphor-logging Entry format and create the log
     * based on device namespace.
     *
     * @param event
     * @return aml::RcCode
     */
    aml::RcCode process([[maybe_unused]] event_info::EventNode& event) override
    {
        bool success = createLog(event);
        if (success)
        {
            return aml::RcCode::succ;
        }
        return aml::RcCode::error;
    }

    /**
     * @brief Return an object path from @c xyz.openbmc_project.ObjectMapper
     * service which corresponds to the given @c deviceId.
     *
     * This will be put in the REDFISH_ORIGIN_OF_CONDITION Log additional's data
     * property.
     *
     * For example, for the "PCIeRetimer_0" given as the @c deviceId a
     * "/xyz/openbmc_project/inventory/system/chassis/HGX_PCIeRetimer_0" path
     * should be returned.
     *
     * If no associated object path could be found return an empty string.
     */
    std::string
        getOriginOfConditionObjectPath(const std::string& deviceId) const;

    /**
     * @brief Return phosphor logging Namespace to be used for log
     *
     * This will be put in the REDFISH_ORIGIN_OF_CONDITION Log additional's data
     * property.
     *
     * For example, for the "PCIeRetimer_0" given as the @c deviceId a
     * "/xyz/openbmc_project/inventory/system/chassis/HGX_PCIeRetimer_0" path
     * should be returned.
     *
     * If no associated object path could be found return an empty string.
     */
    static std::string
        getPhosphorLoggingNamespace(const event_info::EventNode& event)
    {
        std::string pNamespace = event.device;
        if (event.subType != "")
        {
            pNamespace += "_" + event.subType;
        }

        return pNamespace;
    }

    /**
     * @brief Use stored in event node data; Collect event related telemetries
     *        values as diag data.
     *
     * @param event
     * @return std::string&
     */
    static std::string collectDiagData(const event_info::EventNode& event)
    {
        nlohmann::ordered_json output;

        if (!event.trigger.isEmpty())
        {
            output["trigger"] = event.trigger.getDataValue().getString();
        }
        else
        {
            output["trigger"] = "empty";
        }

        if (!event.accessor.isEmpty())
        {
            output["accessor"] = event.accessor.getDataValue().getString();
        }
        else
        {
            output["accessor"] = "empty";
        }

        output["selftest"] = event.selftestReport;

        for (auto telemetry : event.telemetries)
        {
            std::string telemetryName = telemetry[data_accessor::nameKey];
            // using telemetry.read() at Event level to use Event information
            std::string telemetryValue = telemetry.read(event);
            output[telemetryName] = telemetryValue;
        }
        return output.dump();
    }

    /**
     *  Creates the full Phosphor Logging Severity Level string
     *  If necessary converts a Json severity to  Phosphor Logging severity
     *
     * @return the whole Phosphor Logging string
     */
    static std::string makeSeverity(const std::string& severityJson);

  private:
    /**
     * @brief Establishes D-Bus connection and creates log with eventNode
     * data in phosphor-logging
     *
     * @param event
     * @return boolean for whether or not it was a success
     */
    bool createLog(event_info::EventNode& event);

    std::map<std::string, dat_traverse::Device>& dat;
};

} // namespace message_composer
