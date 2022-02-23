
/*
 *
 */

#pragma once

#include "aml.hpp"
#include "event_handler.hpp"
#include "event_info.hpp"
#include "object.hpp"

#include <boost/container/flat_map.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <string>
#include <thread>

namespace event_detection
{

/**
 * @class EventDetection
 * @brief Responsible for catching D-Bus signals and GPIO state change
 * conditions.
 */
class EventDetection : public object::Object
{
  public:
    EventDetection(const std::string& name, event_info::EventMap* eventMap,
                   event_handler::EventHandlerManager* hdlrMgr) :
        object::Object(name),
        _eventMap(eventMap), _hdlrMgr(hdlrMgr)
    {}
    ~EventDetection() = default;

  public:
    void identifyEventCandidate(const std::string& objPath,
                                const std::string& signature,
                                const std::string& property);

    /**
     * @brief Register DBus propertiesChanged handler.
     *
     * @param evtDet
     * @param conn
     * @param iface
     * @return std::unique_ptr<sdbusplus::bus::match_t>
     */
    static std::unique_ptr<sdbusplus::bus::match_t>
        startEventDetection(EventDetection* evtDet,
                            std::shared_ptr<sdbusplus::asio::connection> conn);

    /**
     * @brief Lookup EventNode per the accessor info
     *
     * @param acc
     * @return event_info::EventNode*
     */
    event_info::EventNode*
        LookupEventFrom(const data_accessor::DataAccessor& acc)
    {
        for (auto& devType : *this->_eventMap)
        {
            for (auto& event : devType.second)
            {
                // event.accessor["object"] =
                // "xyz/openbmc_project/sensors/temperature/TEMP_GB_GPU[0-7]"
                // acc["object"]

                if (event.accessor == acc)
                {
                    return &event;
                }
            }
        }
        return nullptr;
    }

    /**
     * @brief Check if the candidate is an event based on Leaky Bucket logic.
     *
     * @param candidate
     * @return true
     * @return false
     */
    bool IsEvent([[maybe_unused]] event_info::EventNode& candidate)
    {
        return true;
    }

    /**
     * @brief Throw out a thread to run all event handlers on the Event.
     *
     * @param event
     */
    void RunEventHandlers(event_info::EventNode& event)
    {
        auto thread = new std::thread(
            [this](event_info::EventNode& _event) {
                this->_hdlrMgr->RunAllHandlers(_event);
            },
            std::ref(event));

        if (thread == nullptr)
        {
            std::cout << "Create thread to process event failed!\n";
        }
    }

  private:
    /**
     * @brief Pointer to the eventMap.
     *
     */
    event_info::EventMap* _eventMap;

    /**
     * @brief Pointer to the eventHanlderManager.
     *
     */
    event_handler::EventHandlerManager* _hdlrMgr;
};

} // namespace event_detection
