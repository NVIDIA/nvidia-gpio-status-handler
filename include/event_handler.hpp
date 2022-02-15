
/**
 *
 */

#pragma once

#include "aml.hpp"
#include "event_info.hpp"
#include "log.hpp"
#include "object.hpp"

#include <memory>
#include <string>
#include <vector>

namespace event_handler
{

/**
 * @brief Base class for all event handlers.
 *
 */
class EventHandler : public object::Object
{
  public:
    EventHandler(const std::string& name = __PRETTY_FUNCTION__) :
        object::Object(name)
    {}

  public:
    virtual aml::RcCode process(event_info::EventNode&) = 0;
};

/**
 * @brief A class for managing all event handlers.
 *
 */
class EventHandlerManager : public object::Object
{
  public:
    EventHandlerManager(const std::string& name = __PRETTY_FUNCTION__) :
        object::Object(name)
    {}
    ~EventHandlerManager() = default;

  public:
    /**
     * @brief Register event handlers.
     *
     * @param hdlr
     */
    void RegisterHandler(EventHandler* hdlr)
    {
        _handlers.emplace_back(hdlr);
        log_dbg("handlers(%s) registered.\n", hdlr->getName().c_str());
    }

    /**
     * @brief Run all registered event handlers in order.
     *
     * @param event
     * @return aml::RcCode
     */
    aml::RcCode RunAllHandlers(event_info::EventNode& event)
    {
        for (auto& hdlr : _handlers)
        {
            log_dbg("running handler(%s) on event(%s).\n",
                    hdlr->getName().c_str(), event.getName().c_str());

            auto rc = hdlr->process(event);

            if (rc != aml::RcCode::succ)
            {
                log_err("handler(%s) on event(%s) failed, rc = %d!\n",
                        hdlr->getName().c_str(), event.getName().c_str(),
                        aml::to_integer(rc));

                // return on first failure.
                // TODO: add option for continue on failure if needed.
                return rc;
            }
        }
        return aml::RcCode::succ;
    }

  private:
    /**
     * @brief hold all event handlers.
     *
     */
    std::vector<EventHandler*> _handlers;
};

} // namespace event_handler
