
/*
 *
 */

#pragma once

#include "aml.hpp"
#include "event_handler.hpp"
#include "event_info.hpp"

#include <string>

namespace event_handler
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
        return aml::RcCode::succ;
    }

  private:
};

} // namespace event_handler
