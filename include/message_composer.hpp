
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
     * @brief Return device path for REDFISH_ORIGIN_OF_CONDITION
     *
     * @param event
     * @return std::string&
     */
    static std::string getDeviceDBusPath(const std::string& device)
    {

        auto bus = sdbusplus::bus::new_default_system();
        auto method = bus.new_method_call("xyz.openbmc_project.ObjectMapper",
                                          "/xyz/openbmc_project/object_mapper",
                                          "xyz.openbmc_project.ObjectMapper",
                                          "GetSubTreePaths");
        int depth = 6;
        method.append("/xyz/openbmc_project", depth,
                      std::vector<std::string>());
        auto reply = bus.call(method);
        std::vector<std::string> dbusPaths;
        reply.read(dbusPaths);

        for (auto& objPath : dbusPaths)
        {
            if (boost::algorithm::ends_with(objPath, "/" + device) ||
                boost::algorithm::ends_with(objPath, "_" + device))
            {
                return objPath;
            }
        }

        std::cerr << "Found no path in ObjectMapper ending with device: "
                  << device << "\n";
        return "";
    }

    /**
     * @brief Compose REDFISH_MESSAGE_ARGS from event
     *
     * @param event
     * @return std::string&
     */
    static std::string createMessageArgs(const event_info::EventNode& event)
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

    /**
     * @brief Use stored in event node data; Collect event related telemetries
     *        values as diag data.
     *
     * @param event
     * @return std::string&
     */
    static std::string collectDiagData(const event_info::EventNode& event)
    {
        nlohmann::json output;
        output["selftest"] = event.selftestReport;

        for (auto telemetry : event.telemetries)
        {
            std::string telemetryName = telemetry[data_accessor::nameKey];
            std::string telemetryValue = telemetry.read();
            output[telemetryName] = telemetryValue;
        }
        return output.dump();
    }

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
