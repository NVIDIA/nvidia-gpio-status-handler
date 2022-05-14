
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

#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace event_detection
{

constexpr int invalidIntParam = -1;
using DbusEventHandlerList =
    std::vector<std::unique_ptr<sdbusplus::bus::match_t>>;

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
     * @brief This is the callback which handles DBUS properties changes
     */
    static void dbusEventHandlerCallback(sdbusplus::message::message& msg);

    /**
     * @brief Register DBus propertiesChanged handler.
     *
     * @param evtDet
     * @param conn
     * @param iface
     * @return a list of std::unique_ptr<sdbusplus::bus::match_t>
     */
    static DbusEventHandlerList
        startEventDetection(EventDetection* evtDet,
                            std::shared_ptr<sdbusplus::asio::connection> conn);

    /**
     * @brief Lookup EventNode per the accessor info, which list all the
     *       asserted EventNode per device instance.
     *
     * @param acc
     * @return event_info::EventNode*
     */

    std::vector<event_info::EventNode>
        LookupEventFrom(const data_accessor::DataAccessor& acc)
    {
        std::vector<event_info::EventNode> eventList;
        for (auto& eventPerDevType : *this->_eventMap)
        {
            for (auto& event : eventPerDevType.second)
            {
#ifdef ENABLE_LOGS
                std::cout << __PRETTY_FUNCTION__
                          << "\n\tevent.accessor: " << event.accessor
                          << "\n\tevent.trigger: " << event.trigger
                          << "\n\tacc: " << acc << "\n";
#endif
                /**
                 * event.trigger is compared first, when it macthes:
                 *   1. event.trigger performs a check against the data from acc
                 *   2. if check passes and event.accessor is not empty
                 *     2.1 event.accessor performs a check against its own data
                 */
                if (event.trigger.isEmpty() == false && event.trigger == acc)
                {
                    if (event.trigger.check(acc, event.deviceType) == true)
                    {
                        if (event.accessor.check(event.deviceType) == true)
                        {
                            /**
                             * the assertedDeviceNames can be in either
                             * event.accessor (most of the times)
                             * or in acc which is the DBUS accessor
                             */
                            event.assertedDeviceNames =
                                event.accessor.getAssertedDeviceNames();
                            if (event.assertedDeviceNames.empty() == true)
                            {
                                event.assertedDeviceNames =
                                    acc.getAssertedDeviceNames();
                            }
                            eventList.push_back(event);
                            continue;
                        }
                    }
                }
                else if (event.accessor == acc)
                {
                    if (event.accessor.check(acc, event.deviceType) == true)
                    {
                        // DataAccessorDBUS property change contains devices
                        event.assertedDeviceNames =
                            acc.getAssertedDeviceNames();
                        eventList.push_back(event);
                        continue;
                    }
                }
#ifdef ENABLE_LOGS
                std::cout << __PRETTY_FUNCTION__
                          << " skipping event :" << event.event << std::endl;
#endif
            }
        }

#ifdef ENABLE_LOGS
        std::cout << __PRETTY_FUNCTION__
                  << " got total events :" << eventList.size() << std::endl;
#endif
        return eventList;
    }

    /**
     * @brief Check if the candidate is an event based on Leaky Bucket logic.
     *
     * @param candidate
     * @return true
     * @return false
     */
    bool IsEvent(event_info::EventNode& candidate,
                 int compareCount = invalidIntParam)
    {
        int count = candidate.count[candidate.device] + 1;
        if (candidate.valueAsCount)
        {
            count = (compareCount == -1) ? std::stoi(candidate.accessor.read())
                                         : compareCount;
        }

        auto countDiff = candidate.triggerCount - count;

        if (countDiff <= 0)
        {
            if (!candidate.valueAsCount)
            {
                candidate.count[candidate.device] = candidate.triggerCount;
            }
            return true;
        }

        if (!candidate.valueAsCount)
        {
            candidate.count[candidate.device]++;
        }
        return false;
    }

    /**
     * @brief Throw out a thread to run all event handlers on the Event.
     *
     * @param event
     */
    void RunEventHandlers(event_info::EventNode& event)
    {
        auto thread = std::make_unique<std::thread>([this, event]() mutable {
#ifdef ENABLE_LOGS
            std::cout << "calling hdlrMgr: " << this->_hdlrMgr->getName()
                      << " event: " << event.event << "\n";
#endif
            auto hdlrMgr = *this->_hdlrMgr;
            hdlrMgr.RunAllHandlers(event);
        });

        if (thread != nullptr)
        {
            thread->detach(); // separate, this no longer owns this thread
        }
        else
        {
#ifdef ENABLE_LOGS
            std::cout << "Create thread to process event failed!\n";
#endif
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
