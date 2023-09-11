/*
 *
 */
#include "event_detection.hpp"

#include "aml.hpp"
#include "aml_main.hpp"
#include "data_accessor.hpp"
#include "event_handler.hpp"
#include "event_info.hpp"
#include "pc_event.hpp"

#include <boost/container/flat_map.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <chrono>
#include <iostream>
#include <regex>
#include <thread>

using json = nlohmann::json;

using namespace std;

namespace event_detection
{

/**
 * @brief keeps the main EventDetection pointer received by startEventDetection
 */
static EventDetection* eventDetectionPtr = nullptr;

/**
 * @brief keeps track of events that have been detected
 */
std::map<std::string, std::vector<std::string>> eventsDetected;

std::unique_ptr<ThreadpoolManager> threadpoolManager;

std::unique_ptr<PcQueueType> queue;

event_info::EventTriggerView eventTriggerView;
event_info::EventAccessorView eventAccessorView;
event_info::EventRecoveryView eventRecoveryView;

void EventDetection::workerThreadProcessEvents()
{
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        PcDataType pc;
        if (queue->pop(pc))
        {
            data_accessor::DataAccessor accessor = pc.accessor;
            data_accessor::PropertyValue propertyValue =
                accessor.getDataValue();
            std::vector<std::shared_ptr<event_info::EventNode>> eventPtrs =
                pc.eventPtrs;

            auto eventsCandidateList =
                eventDetectionPtr->EventsDetection(accessor, eventPtrs);

            if (eventsCandidateList.empty() == true)
            {
                logs_dbg("No event found in the supporting list.\n");
                continue;
            }
            std::stringstream ss;
            accessor.print(ss);
            logs_err(
                "Event Candidate List has events from PC Trigger/Accessor %s\n",
                ss.str().c_str());
            logs_err("Data value: %s \n", propertyValue.getString().c_str());
            for (auto& assertedEvent : eventsCandidateList)
            {
                auto& candidate = *std::get<0>(assertedEvent);
                logs_err("Asserted Event: %s\n", candidate.event.c_str());
                int eventValue = invalidIntParam;
                if (candidate.valueAsCount)
                {
                    eventValue = propertyValue.getInteger();
                }
                const auto& assertedDeviceList = std::get<1>(assertedEvent);
                bool isRecovery = std::get<2>(assertedEvent);
                if (assertedDeviceList.empty() == true)
                {
                    logs_err("event: '%s' no asserted devices, exiting...\n",
                             candidate.event.c_str());
                    continue;
                }
                for (const auto& assertedDevice : assertedDeviceList)
                {
                    candidate.device = assertedDevice.device;
                    auto event = candidate; // keep it a copy
                    event.trigger = assertedDevice.trigger;
                    event.accessor = assertedDevice.accessor;
                    event.setDeviceIndexTuple(assertedDevice.deviceIndexTuple);
                    
                    if (isRecovery)
                    {
                        logs_err(
                            "event: '%s' on dev %s doing recovery...running event handlers\n",
                            event.event.c_str(), event.device.c_str());
                        eventDetectionPtr->RunEventHandler(event,
                                                           "RootCauseTracer");
                        continue;
                    }

                    if (eventDetectionPtr->IsEvent(candidate, eventValue))
                    {
                        event.severities.push_back(event.getSeverity());
                        auto currentDeviceHealth = util::getDeviceHealth(event.device);

                        if (!currentDeviceHealth.empty())
                        {
                            event.severities.push_back(currentDeviceHealth);
                        }
                        else
                        {
                            logs_err("Failed to get current device health (ignored)\n");
                        }

                        std::stringstream ss;
                        ss << "Throw out an eventHdlrMgr. device: "
                           << event.device << " event: '" << event.event << "'"
                           << " deviceIndex: "
                           << assertedDevice.deviceIndexTuple;
                        logs_err("%s\n", ss.str().c_str());
                        eventDetectionPtr->RunEventHandlers(event);
                        logs_dbg(
                            "Adding event %s to internal map with afflicted device %s\n",
                            candidate.event.c_str(), event.device.c_str());

                        auto eventKey =
                            candidate.event + candidate.getMainDeviceType();
                        if (eventsDetected.count(eventKey) == 0)
                        {
                            eventsDetected.insert(
                                std::pair<std::string,
                                          std::vector<std::string>>(
                                    eventKey,
                                    std::vector<std::string>{event.device}));
                        }
                        else
                        {
                            eventsDetected.at(eventKey).push_back(event.device);
                        }
                    }
                }
            }
        }
    }
}

void EventDetection::workerThreadMainLoop()
{
    try
    {
        auto bus = sdbusplus::bus::new_default_system();
        logs_err("thread2: starting event processing\n");
        workerThreadProcessEvents();
        logs_err(
            "thread2: exiting because workerThreadProcessEvents returned!!\n");
    }
    catch (const std::exception& e)
    {
        logs_err("thread2: %s\n", e.what());
        return;
    }
}

void EventDetection::dbusEventHandlerCallback(sdbusplus::message::message& msg)
{
    logs_dbg("entered dbusEventHandlerCallback\n");
    std::string msgInterface;
    boost::container::flat_map<std::string, PropertyVariant> propertiesChanged;

    msg.read(msgInterface, propertiesChanged);

    std::string objectPath = msg.get_path();
    std::string sender = msg.get_sender();

    if (propertiesChanged.empty())
    {
        logs_err("sdbusplus::message: empty propertiesChanged, return.\n");
        return;
    }

    // pcTimestampType timestamp = std::chrono::steady_clock::now();

    for (auto& pc : propertiesChanged)
    {
        /**
         * Example
         *
            signal time=1645151704.737424 sender=org.freedesktop.DBus ->
         destination=:1.93 serial=4294967295 path=/org/freedesktop/DBus;
         interface=org.freedesktop.DBus; member=NameAcquired string
         ":1.93" method call time=1645151704.762798 sender=:1.93 ->
         destination=xyz.openbmc_project.GpuMgr serial=2
         path=/xyz/openbmc_project/inventory/system/chassis/GPU0;
         interface=org.freedesktop.DBus.Properties; member=Get string
         "xyz.openbmc_project.Inventory.Decorator.Dimension" string
         "Depth" method return time=1645151704.764463 sender=:1.58 ->
         destination=:1.93 serial=6777 reply_serial=2 variant double 100
         *
         */
        auto variant = pc.second;
        std::string eventProperty = pc.first;

        auto index = variant.index();
        if (eventProperty.empty() || isValidVariant(variant) == false)
        {
            logs_err("[sdbusplus::message] empty or invalid Property, skipping "
                     "Path: %s, Intf: %s, Prop: '%s', VarIndex: %d\n",
                     objectPath.c_str(), msgInterface.c_str(),
                     eventProperty.c_str(), index);
            continue;
        }

        // TODO: the json, propertyvalue, and dataaccessor creation are
        // duplicated here and on the second thread.
        const std::string type = "DBUS";
        nlohmann::json j;
        j[data_accessor::typeKey] = type;
        j[data_accessor::accessorTypeKeys[type][0]] = objectPath;
        j[data_accessor::accessorTypeKeys[type][1]] = msgInterface;
        j[data_accessor::accessorTypeKeys[type][2]] = eventProperty;
        data_accessor::PropertyValue propertyValue(variant);
        data_accessor::DataAccessor accessor(j, propertyValue);

        logs_dbg(
            "Got PC Trigger ... Path: %s, Intf: %s, Prop: '%s', VarIndex: %d\n",
            objectPath.c_str(), msgInterface.c_str(), eventProperty.c_str(),
            index);
        logs_dbg("Passing PC Trigger into Event Discovery phase\n");

        eventDiscovery(accessor);

    } // end for (auto& pc : propertiesChanged)
    logs_dbg("finished dbusEventHandlerCallback\n");
}

DbusEventHandlerList EventDetection::startEventDetection(
    EventDetection* eventDetection,
    std::shared_ptr<sdbusplus::asio::connection> conn)
{
    // it will be used in EventDetection::dbusEventHandlerCallback()
    eventDetectionPtr = eventDetection;

    DbusEventHandlerList handlerList;
    // helper map to make sure a object-path+interface is registered just once
    RegisteredObjectInterfaceMap uniqueRegisterMap;

    for (const auto& dev : *this->_eventMap)
    {
        for (auto& event : dev.second)
        {
            subscribeAcc(event.trigger, uniqueRegisterMap, conn, handlerList);
            subscribeAcc(event.accessor, uniqueRegisterMap, conn, handlerList);
            subscribeAcc(event.recovery_accessor, uniqueRegisterMap, conn,
                         handlerList);
        }
    }
    log_dbg("dbusEventHandlerMatcher created.\n");
    return handlerList;
}

void EventDetection::subscribeAcc(
    const data_accessor::DataAccessor& acc, RegisteredObjectInterfaceMap& map,
    std::shared_ptr<sdbusplus::asio::connection>& conn,
    DbusEventHandlerList& handlerList)
{
    auto genericHandler = std::bind(&EventDetection::dbusEventHandlerCallback,
                                    std::placeholders::_1);
    auto dbusInfo = acc.getDbusInterfaceObjectsMap();
    if (dbusInfo.empty() == false)
    {
        const auto& interface = dbusInfo.begin()->first;
        size_t i = 0;
        for (const auto& object : dbusInfo.begin()->second)
        {
            auto key = dbusInfo.begin()->second.at(i) + ":" + interface;
            if (map.count(key) == 0)
            {
                log_dbg("subscribing object:interface : %s\n", key.c_str());
                handlerList.push_back(dbus::registerServicePropertyChanged(
                    conn, object, interface, genericHandler));
                map[key] = 1; // global map, indicate it is already subscribed
            }
            else
            {
                log_dbg("object:interface already subscribed: %s\n",
                        key.c_str());
            }
            i++;
        }
    }
}

void pushToQueue(const data_accessor::DataAccessor& pcTrigger,
                 std::vector<std::shared_ptr<event_info::EventNode>> eventPtrs)
{
    bool pushSuccess =
        queue->push(PcDataType{.accessor = pcTrigger, .eventPtrs = eventPtrs});
    if (!pushSuccess)
    {
        logs_err("callback: failed to push event to queue! \n");
        std::stringstream ss;
        pcTrigger.print(ss);
        logs_err("PC Trigger Accessor contents: %s\n", ss.str().c_str());
        logs_err("PC Trigger Data value: %s \n",
                 pcTrigger.getDataValue().getString().c_str());
    }
    size_t availableSpace = queue->write_available();
    logs_err("callback: queue now has space for %zu elements\n",
             availableSpace);
    if (availableSpace < PROPERTIESCHANGED_QUEUE_SIZE / 2)
    {
        logs_err("callback: queue has less than 50%% space available "
                 "(%zu available, %zu total)\n",
                 availableSpace, (size_t)PROPERTIESCHANGED_QUEUE_SIZE);
    }
}

void EventDetection::eventDiscovery(const data_accessor::DataAccessor& accessor,
                                    const bool& bootup)
{
    std::vector<std::shared_ptr<event_info::EventNode>> eventPtrs;
    std::stringstream ss;
    accessor.print(ss);
    if (!bootup)
    {
        logs_dbg("In Event Discovery phase for pc Trigger %s\n",
                 ss.str().c_str());
        auto itr = eventTriggerView.equal_range(accessor);
        if (itr.first == itr.second)
        {
            logs_dbg("PC Trigger %s not found in eventTriggerView\n",
                     ss.str().c_str());
            return;
        }
        for (auto it = itr.first; it != itr.second; it++)
        {
            logs_dbg("Discovered event with matching trigger: %s\n",
                     it->second->event.c_str());
            eventPtrs.push_back(it->second);
        }
    }
    else
    {
        logs_err("In Bootup Event Detection phase for accessor!!! %s\n",
                 ss.str().c_str());
        auto itr = eventAccessorView.equal_range(accessor);
        if (itr.first == itr.second)
        {
            logs_err("Accessor %s not found in eventAccessorView\n",
                     ss.str().c_str());
            return;
        }
        for (auto it = itr.first; it != itr.second; it++)
        {
            logs_err("Discovered bootup event with matching accessor: %s\n",
                     it->second->event.c_str());
            eventPtrs.push_back(it->second);
        }
    }
    pushToQueue(accessor, eventPtrs);
}

bool EventDetection::getIsAccessorInteresting(
    const data_accessor::DataAccessor& accessor)
{
    if (accessor.isEmpty())
    {
        log_dbg("empty accessor is never interesting\n");
        return false;
    }
    if (accessor.isTypeDbus())
    {
        // the set stores elements in reverse order for comparison efficiency
        auto eventProperty = accessor.getProperty();
        auto msgInterface = accessor.getDbusInterface();
        auto objectPath = accessor.getDbusObjectPath();
        return _propertyFilterSet->contains(
            {eventProperty, msgInterface, objectPath});
    }
    else
    {
        for (auto& eventPerDevType : *eventDetectionPtr->_eventMap)
        {
            for (auto& event : eventPerDevType.second)
            {
                if (event_info::EventNode::getIsAccessorInterestingToEvent(
                        event, accessor))
                {
                    logs_err("Event %s has interesting accessor!\n",
                             event.event.c_str());
                    return true;
                }
            }
        }
        return false;
    }
}

#if 0
void EventDetection::identifyEventCandidate(const std::string& objPath,
                                            const std::string& signature,
                                            const std::string& property)
{
    logs_info("dbus([%s]/[%s]/[%s]).\n", objPath.c_str(), signature.c_str(), property.c_str());
    std::regex rgx("obj=\"(.+?)\"");
    std::smatch match;

    for (const auto& dev : event_detection::EventDetection::eventMap)
    {
        // std::cerr << dev.first << " events:\n";

        for (const auto& event : dev.second)
        {
            // std::cerr << event.event << "\n";
            if (std::regex_search(event.accessor.metadata.begin(),
                                  event.accessor.metadata.end(),
                                  match, rgx))
            {
                std::string eventPath = match[1];

                if (objPath.find(eventPath) != std::string::npos)
                {
                    log_dbg("Matched event: %s %s\n", objPath.c_str(), eventPath.c_str());
                    goto exit;
                }
            }
            else
            {
                log_err("Regex Issue\n");
                goto exit;
            }
        }
    }
exit:

    log_err("Exit\n");
}
#endif
// void EventDetection::~EventDetection()
// {
// }

} // namespace event_detection
