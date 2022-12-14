
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
 *  This list is used in LookupEventFrom(),
 *
 *  Per each event in the list there is a assertedDevices map assigned to it
 */
using AssertedEventList =
         std::vector<std::pair<event_info::EventNode*, util::DeviceIdMap>>;

/**
 *   A map to check if a pair object-path + interface has already been
 *    registered for receiving PropertyChange signal from dbus
 *
 *   if a object-path + interface is registred more than once, more than once
 *    message will arrive.
 */
using  RegisteredObjectInterfaceMap = std::map<const std::string, int>;


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
    DbusEventHandlerList startEventDetection(EventDetection* evtDet,
                            std::shared_ptr<sdbusplus::asio::connection> conn);

    /**
     * @brief Lookup EventNode per the accessor info, which list all the
     *       asserted EventNode per device instance.
     *
     * @param acc
     * @return event_info::EventNode*
     */

    /**
     * @brief LookupEventFrom
     * @param acc
     * @return
     */
    AssertedEventList LookupEventFrom(const data_accessor::DataAccessor& acc)
    {
        AssertedEventList eventList;
        for (auto& eventPerDevType : *this->_eventMap)
        {
            for (auto& event : eventPerDevType.second)
            {
                std::stringstream ss;
                ss << "\n\tevent.accessor: " << event.accessor
                    << "\n\tevent.trigger: " << event.trigger
                    << "\n\tacc: " << acc;
                log_dbg("%s\n", ss.str().c_str());

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
                        // the Accessor which receives the check should always
                        // be a temporary object as it keeps data from read()
                        // and assertedDevices list
                        auto tmpAccessor = event.accessor;
                        if (tmpAccessor.check(acc.getAssertedDeviceNames(),
                                              event.deviceType) == true)
                        {
                            auto assertedDevices =
                                    tmpAccessor.getAssertedDeviceNames();
                            /**
                             * the assertedDeviceNames can be in either
                             * event.accessor (most of the times)
                             * or in acc which is the DBUS accessor
                             */
                            if (assertedDevices.empty() == true)
                            {
                                assertedDevices = acc.getAssertedDeviceNames();
                            }
                            eventList.push_back(std::make_pair(
                                                      &event, assertedDevices));
                            continue;
                        }
                    }
                }
                else if (event.accessor == acc)
                {
                    if (event.accessor.check(acc, event.deviceType) == true)
                    {
                        eventList.push_back(std::make_pair(
                                       &event,  acc.getAssertedDeviceNames()));
                        continue;
                    }
                }
                ss.str(std::string()); // Clear the stream
                ss << "skipping event :" << event.event;
                log_dbg("%s\n", ss.str().c_str());
            }
        }
        std::stringstream ss;
        ss << "got total events :" << eventList.size();
        log_dbg("%s\n", ss.str().c_str());
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

        if (candidate.triggerCount == -1)
        {
            return false;
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
            std::stringstream ss;
            ss << "calling hdlrMgr: " << this->_hdlrMgr->getName()
                      << " event: " << event.event;
            log_dbg("%s\n", ss.str().c_str());
            auto hdlrMgr = *this->_hdlrMgr;
            hdlrMgr.RunAllHandlers(event);
        });

        if (thread != nullptr)
        {
            thread->detach(); // separate, this no longer owns this thread
        }
        else
        {
            log_err("Create thread to process event failed!\n");
        }
    }

  private:
    /**
     * @brief  Subscribe Accessor Dbus objects to receive PropertyChanged signal
     * @param  acc the DataAccessor
     * @param  map a global map for all events to make sure unique subscribing
     *           for a single object + interface
     * @param  conn the current Dbus IO connection
     * @param  The handler list of subscribing to be populated
     */
    void subscribeAcc(const data_accessor::DataAccessor& acc,
                      RegisteredObjectInterfaceMap& map,
                      std::shared_ptr<sdbusplus::asio::connection>& conn,
                      DbusEventHandlerList& handlerList);

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
