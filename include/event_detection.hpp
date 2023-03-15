
/*
 *
 */

#pragma once

#include "aml.hpp"
#include "check_accessor.hpp"
#include "dbus_accessor.hpp"
#include "event_handler.hpp"
#include "event_info.hpp"
#include "object.hpp"
#include "pc_event.hpp"
#include "threadpool_manager.hpp"

#include <boost/container/flat_map.hpp>
#include <dbus_log_utils.hpp>
#include <dbus_utility.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <variant>

#ifndef PROPERTIESCHANGED_QUEUE_SIZE
#define PROPERTIESCHANGED_QUEUE_SIZE 100
#endif

namespace event_detection
{

extern std::map<std::string, std::vector<std::string>> eventsDetected;
extern std::unique_ptr<PcQueueType> queue;

extern std::unique_ptr<ThreadpoolManager> threadpoolManager;

constexpr int invalidIntParam = -1;
using DbusEventHandlerList =
    std::vector<std::unique_ptr<sdbusplus::bus::match_t>>;

/**
 *  This list is used in LookupEventFrom(),
 *
 *  Each event carries an assertedDeviceList (list of devices and their data)
 */
using AssertedEventList = std::vector<
    std::pair<event_info::EventNode*, data_accessor::AssertedDeviceList>>;

/**
 *   A map to check if a pair object-path + interface has already been
 *    registered for receiving PropertyChange signal from dbus
 *
 *   if a object-path + interface is registred more than once, more than once
 *    message will arrive.
 */
using RegisteredObjectInterfaceMap = std::map<const std::string, int>;

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
     * @brief This is the main function of the worker thread
     */
    static void workerThreadMainLoop();

    /**
     * @brief This is the loop which actually processes PropertiesChanged messages
     */
    static void workerThreadProcessEvents();

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
    DbusEventHandlerList
        startEventDetection(EventDetection* evtDet,
                            std::shared_ptr<sdbusplus::asio::connection> conn);

    /**
     * @brief Perform recovery actions for a fault HMC recovers from.
     *
     * @param eventName
     * @param deviceType
     */
    void recoverFromFault(const std::string& eventName,
                          const std::string& deviceType)
    {
        std::string eventKey = eventName + deviceType;
        if (eventsDetected.count(eventKey) > 0)
        {
            for (const auto& dev : eventsDetected.at(eventKey))
            {
                log_dbg("Performing recovery on device %s for event %s\n",
                        dev.c_str(), eventName.c_str());
                updateDeviceHealthAndResolve(dev, eventName);
            }
            eventsDetected.at(eventKey).clear();
        }
    }

    /**
     * @brief  Reset device health on DBus back to OK
     *
     * @param devId
     */
    void resetDeviceHealth(const std::string& devId)
    {
        dbus::DirectObjectMapper om;
        const std::string healthInterface(
            "xyz.openbmc_project.State.Decorator.Health");
        std::vector<std::string> objPathsToAlter =
            om.getAllDevIdObjPaths(devId, healthInterface);
        if (!objPathsToAlter.empty())
        {
            std::string healthState =
                "xyz.openbmc_project.State.Decorator.Health.HealthType.OK";
            for (const auto& objPath : objPathsToAlter)
            {
                log_dbg("Setting Health Property for: %s healthState: %s\n",
                        objPath.c_str(), healthState.c_str());
                bool ok = dbus::setDbusProperty(
                    objPath, "xyz.openbmc_project.State.Decorator.Health",
                    "Health", PropertyVariant(healthState));
                if (ok == true)
                {
                    log_dbg("Changed health property as expected\n");
                }
            }
        }
    }

    /**
     * @brief  Sets Resolved property of log entry to true
     *
     * @param objPath
     * @param bus
     */
    void setLogEntryResolved(const std::string objPath,
                             sdbusplus::bus::bus& bus)
    {
        try
        {
            std::variant<bool> v = true;
            dbus::DelayedMethod method(
                bus, "xyz.openbmc_project.Logging", objPath,
                "org.freedesktop.DBus.Properties", "Set");
            method.append("xyz.openbmc_project.Logging.Entry");
            method.append("Resolved");
            method.append(v);
            auto reply = method.call();
        }
        catch (const sdbusplus::exception::exception& e)
        {
            log_err(" Dbus Error: %s\n", e.what());
            throw std::runtime_error(e.what());
        }
    }

    /**
     * @brief  Recover from events which can only be recovered after power cycle
     *
     * @param devId
     * @param result
     * @param bus
     */
    void recoverFromPowerCycleEvents(
        const std::string& devId,
        const dbus::utility::ManagedObjectType& result,
        sdbusplus::bus::bus& bus)
    {
        for (auto& objectPath : result)
        {
            bool resolved = false;
            const std::vector<std::string>* additionalDataRaw = nullptr;

            for (auto& interfaceMap : objectPath.second)
            {
                if (interfaceMap.first == "xyz.openbmc_project.Logging.Entry")
                {
                    for (auto& propertyMap : interfaceMap.second)
                    {
                        if (propertyMap.first == "AdditionalData")
                        {
                            additionalDataRaw =
                                std::get_if<std::vector<std::string>>(
                                    &propertyMap.second);
                        }
                        else if (propertyMap.first == "Resolved")
                        {
                            const bool* resolveptr =
                                std::get_if<bool>(&propertyMap.second);
                            if (resolveptr == nullptr)
                            {
                                log_dbg("Resolved Property pointer is null\n");
                                return;
                            }
                            resolved = *resolveptr;
                        }
                    }
                }
            }

            bool powerCycle = true;
            redfish::AdditionalData additional(*additionalDataRaw);
            if (additional.count("RECOVERY_TYPE") > 0)
            {
                powerCycle = additional["RECOVERY_TYPE"] !=
                                     std::string("property_change")
                                 ? true
                                 : false;
            }

            if (!resolved && powerCycle)
            {
                setLogEntryResolved(objectPath.first.str, bus);
            }
        }
        resetDeviceHealth(devId);
    }

    /**
     * @brief Update recovered device's health/healthrollup and log's Resolved
     * DBus property
     *
     * @param devId
     * @param eventName
     */
    void updateDeviceHealthAndResolve(const std::string& devId,
                                      const std::string& eventName)
    {
        auto bus = sdbusplus::bus::new_default_system();
        dbus::utility::ManagedObjectType result;
        try
        {

            dbus::DelayedMethod method(bus, "xyz.openbmc_project.Logging",
                                       "/xyz/openbmc_project/logging",
                                       "xyz.openbmc_project.Logging.Namespace",
                                       "GetAll");
            method.append(devId);
            method.append(
                "xyz.openbmc_project.Logging.Namespace.ResolvedFilterType.Both");
            auto reply = method.call();
            reply.read(result);
        }
        catch (const sdbusplus::exception::exception& e)
        {
            log_err(" Dbus Error: %s\n", e.what());
            throw std::runtime_error(e.what());
        }

        if (eventName.length() == 0)
        {
            recoverFromPowerCycleEvents(devId, result, bus);
            return;
        }

        const std::vector<std::string>* additionalDataRaw = nullptr;
        bool resolved = false;
        for (auto& objectPath : result)
        {
            for (auto& interfaceMap : objectPath.second)
            {
                if (interfaceMap.first == "xyz.openbmc_project.Logging.Entry")
                {
                    for (auto& propertyMap : interfaceMap.second)
                    {
                        if (propertyMap.first == "AdditionalData")
                        {
                            additionalDataRaw =
                                std::get_if<std::vector<std::string>>(
                                    &propertyMap.second);
                        }
                        else if (propertyMap.first == "Resolved")
                        {
                            const bool* resolveptr =
                                std::get_if<bool>(&propertyMap.second);
                            if (resolveptr == nullptr)
                            {
                                log_dbg("Resolved Property pointer is null\n");
                                return;
                            }
                            resolved = *resolveptr;
                        }
                    }
                }
            }

            std::string messageArgs;
            std::string addlEventName;
            if (!resolved && additionalDataRaw != nullptr)
            {
                redfish::AdditionalData additional(*additionalDataRaw);
                if (additional.count("REDFISH_MESSAGE_ARGS") > 0)
                {
                    messageArgs = additional["REDFISH_MESSAGE_ARGS"];
                }
                if (additional.count("EVENT_NAME") > 0)
                {
                    addlEventName = additional["EVENT_NAME"];
                }
                if (eventName == addlEventName &&
                    messageArgs.find(devId) != std::string::npos)
                {
                    resetDeviceHealth(devId);
                    setLogEntryResolved(objectPath.first.str, bus);
                    return;
                }
            }
        }
    }

    /**
     * @brief Lookup EventNode per the accessor info, which list all the
     *       asserted EventNode per device instance.
     *
     * @param acc
     * @return event_info::EventNode*
     */

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
                auto deviceType = event.getStringifiedDeviceType();
                std::stringstream ss;
                ss << "\n\tevent.event: " << event.event
                   << "\n\tevent.deviceType: " << event.getMainDeviceType()
                   << "\n\tevent.device: " << event.device
                   << "\n\tevent.accessor: " << event.accessor
                   << "\n\tevent.trigger: " << event.trigger
                   << "\n\tacc: " << acc;
                log_dbg("%s\n", ss.str().c_str());

                /**
                 * event.trigger is compared first, when it macthes:
                 *   1. event.trigger performs a check against the data from acc
                 *   2. if check passes and event.accessor is not empty
                 *     2.1 event.accessor reads its data
                 */
                const auto& trigger = acc;
                bool checkPerformed = false;
                if (!event.trigger.isEmpty() && event.trigger == trigger)
                {
                    data_accessor::CheckAccessor triggerCheck(deviceType);
                    const auto& accessor = event.trigger;
                    checkPerformed = true;
                    if (triggerCheck.check(accessor, trigger))
                    {
                        data_accessor::CheckAccessor accCheck(deviceType);
                        /* Note: accCheck uses info stored in triggerCheck
                         *   1. the original trigger
                         *   2. Maybe needs triggerCheck asserted devices data
                         *      for example the event "DRAM Contained ECC Error"
                         */
                        // redefines accessor
                        const auto& accessor = event.accessor;
                        if (accCheck.check(accessor, triggerCheck))
                        {
                            eventList.push_back(std::make_pair(
                                &event, accCheck.getAssertedDevices()));
                            continue;
                        }
                    }
                }
                else if (event.accessor == trigger)
                {
                    data_accessor::CheckAccessor accCheck(deviceType);
                    const auto& accessor = event.accessor;
                    checkPerformed = true;
                    if (accCheck.check(accessor, trigger))
                    {
                        eventList.push_back(std::make_pair(
                            &event, accCheck.getAssertedDevices()));
                        continue;
                    }
                }
                if (!event.recovery_accessor.isEmpty() && checkPerformed)
                {
                    log_dbg("Checking content of events detected map\n");
                    for (const auto& e : eventsDetected)
                    {
                        log_dbg("Event + devtype in event map: %s\n",
                                e.first.c_str());
                        for (const auto& dev : e.second)
                        {
                            log_dbg("Dev in map for event+devtype %s: %s\n",
                                    e.first.c_str(), dev.c_str());
                        }
                    }
                    log_dbg(
                        "Recovering from property-changed signal fault %s\n",
                        event.event.c_str());
                    recoverFromFault(event.event, event.getMainDeviceType());
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
        log_err("entered RunEventHandlers\n");
        auto guard = std::make_shared<ThreadpoolGuard>(threadpoolManager.get());
        if (!guard->was_successful())
        {
            // the threadpool has reached the max queued tasks limit,
            // don't run this event thread
            log_err(
                "Thread pool over maxTotal tasks limit, not creating thread\n");
            log_err("finished RunEventHandlers\n");
            return;
        }
        // capturing guard in the lambda is needed to extend the lifetime
        // of the ThreadpoolGuard so that it only gets destructed
        // (and the thread slot released) when both this trigger function
        // and the thread have finished executing.
        auto thread = std::make_unique<std::thread>([this, event, guard]() mutable {
            log_err("started event thread\n");
            std::stringstream ss;
            ss << "calling hdlrMgr: " << this->_hdlrMgr->getName()
               << " event: " << event.event;
            log_dbg("%s\n", ss.str().c_str());
            auto hdlrMgr = *this->_hdlrMgr;
            hdlrMgr.RunAllHandlers(event);
            log_err("finished event thread\n");
        });

        if (thread != nullptr)
        {
            thread->detach(); // separate, this no longer owns this thread
        }
        else
        {
            log_err("Create thread to process event failed!\n");
        }
        log_err("finished RunEventHandlers\n");
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
