
/*
 *
 */

#pragma once

#include "aml.hpp"
#include "event_handler.hpp"
#include "event_info.hpp"
#include "check_accessor.hpp"
#include "object.hpp"
#include "threadpool_manager.hpp"

#include <boost/container/flat_map.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace event_detection
{

extern std::unique_ptr<ThreadpoolManager> threadpoolManager;

constexpr int invalidIntParam = -1;
using DbusEventHandlerList =
    std::vector<std::unique_ptr<sdbusplus::bus::match_t>>;

/**
 *  This list is used in LookupEventFrom(),
 *
 *  Each event carries an assertedDeviceList (list of devices and their data)
 */
using AssertedEventList =
       std::vector<std::pair<event_info::EventNode*,
                                            data_accessor::AssertedDeviceList>>;

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
     * @brief LookupEventFrom
     * @param acc the DataAccessor which came from DBus and will trigger Events
     * @return an AssertedEventList with the Events to be generated
     */
    AssertedEventList LookupEventFrom(const data_accessor::DataAccessor& acc)
    {
        AssertedEventList eventList;
        for (auto& eventPerDevType : *this->_eventMap)
        {
            for (auto& event : eventPerDevType.second)
            {
                auto& deviceType = event.deviceType;
                std::stringstream ss;
                ss << "\n\tevent.accessor: " << event.accessor
                    << "\n\tevent.trigger: " << event.trigger
                    << "\n\tacc: " << acc;
                log_dbg("%s\n", ss.str().c_str());

                /**
                 * event.trigger is compared first, when it macthes:
                 *   1. event.trigger performs a check against the data from acc
                 *   2. if check passes and event.accessor is not empty
                 *     2.1 event.accessor reads its data
                 */
                const auto& trigger  = acc;
                if (!event.trigger.isEmpty() && event.trigger == trigger)
                {
                    data_accessor::CheckAccessor triggerCheck;
                    const auto& accessor = event.trigger;
                    if (triggerCheck.check(accessor, trigger, deviceType))
                    {
                        data_accessor::CheckAccessor accCheck;
                        /* Note: accCheck uses info stored in triggerCheck
                         *   1. the original trigger
                         *   2. Maybe needs triggerCheck asserted devices data
                         *      for example the event "DRAM Contained ECC Error"
                         */
                        // redefines accessor
                        const auto& accessor = event.accessor;
                        if (accCheck.check(accessor, triggerCheck, deviceType))
                        {
                            eventList.push_back(std::make_pair(
                                        &event, accCheck.getAssertedDevices()));
                            continue;
                        }
                    }
                }
                else if (event.accessor == trigger)
                {
                    data_accessor::CheckAccessor accCheck;
                    const auto& accessor = event.accessor;
                    if (accCheck.check(accessor, trigger, deviceType))
                    {
                        eventList.push_back(std::make_pair(
                                        &event, accCheck.getAssertedDevices()));
                        continue;
                    }
                }
                log_dbg("skipping event : %s\n", event.event.c_str());
            }
        }
        log_dbg("got total events : %lu\n", eventList.size());
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
            log_wrn("started event thread\n");
            ThreadpoolGuard guard(threadpoolManager.get());
            if (!guard.was_successful())
            {
                // the threadpool has reached the max queued tasks limit,
                // don't run this event thread
                log_err("Thread pool over maxTotal tasks limit, exiting event thread\n");
                return;
            }
            std::stringstream ss;
            ss << "calling hdlrMgr: " << this->_hdlrMgr->getName()
                      << " event: " << event.event;
            log_dbg("%s\n", ss.str().c_str());
            auto hdlrMgr = *this->_hdlrMgr;
            hdlrMgr.RunAllHandlers(event);
            log_wrn("finished event thread\n");
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
