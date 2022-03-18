
/*
 *
 */

#pragma once

#include "aml.hpp"
#include "data_accessor.hpp"
#include "event_handler.hpp"
#include "event_info.hpp"

#include <nlohmann/json.hpp>

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
    MessageComposer(const std::string& name = __PRETTY_FUNCTION__) :
        event_handler::EventHandler(name)
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
     * @brief Collect event related telemetries values as diag data.
     *
     * @param event
     * @return std::string&
     */
    static std::string collectDiagData(const event_info::EventNode& event)
    {
        nlohmann::json output;
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
};

} // namespace message_composer
