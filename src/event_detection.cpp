
/*
 *
 */
#include "event_detection.hpp"

#include "aml.hpp"
#include "aml_main.hpp"
#include "data_accessor.hpp"

#include <boost/container/flat_map.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <iostream>
#include <regex>

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

void EventDetection::dbusEventHandlerCallback(sdbusplus::message::message& msg)
{
    ThreadpoolGuard guard(threadpoolManager.get());
    if (!guard.was_successful())
    {
        // the threadpool has reached the max queued tasks limit,
        // this D-Bus signal will be ignored
        logs_err(
            "Thread pool over maxTotal tasks limit, exiting dbusEventHandlerCallback\n");
        return;
    }
    std::string msgInterface;
    boost::container::flat_map<std::string, PropertyVariant> propertiesChanged;

    msg.read(msgInterface, propertiesChanged);

    std::string objectPath = msg.get_path();

    if (propertiesChanged.empty())
    {
        logs_err("sdbusplus::message: empty propertiesChanged, return.\n");
        return;
    }

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

        data_accessor::PropertyValue propertyValue(variant);
        const std::string type = "DBUS";
        nlohmann::json j;
        j[data_accessor::typeKey] = type;
        j[data_accessor::accessorTypeKeys[type][0]] = objectPath;
        j[data_accessor::accessorTypeKeys[type][1]] = msgInterface;
        j[data_accessor::accessorTypeKeys[type][2]] = eventProperty;
        data_accessor::DataAccessor accessor(j, propertyValue);
        auto assertedEventsList = eventDetectionPtr->LookupEventFrom(accessor);
        if (assertedEventsList.empty() == true)
        {
            logs_dbg("No event found in the supporting list.\n");
            continue;
        }
        // printing DBUS trigger here as one trigger can generate several events
        logs_err("[sdbusplus::message] sender: %s, Path: %s, "
                 " Intf: %s, Prop: %s,VarIndex: %d, Value: '%s'\n",
                 msg.get_sender(), objectPath.c_str(), msgInterface.c_str(),
                 eventProperty.c_str(), index,
                 propertyValue.getString().c_str());

        for (auto& assertedEvent : assertedEventsList)
        {
            auto& candidate = *assertedEvent.first;
            int eventValue = invalidIntParam;
            if (candidate.valueAsCount)
            {
                eventValue = propertyValue.getInteger();
            }
            const auto& assertedDeviceList = assertedEvent.second;
            if (assertedDeviceList.empty() == true)
            {
                logs_err("event: '%s' no asserted devices, exiting...\n",
                         candidate.event.c_str());
                continue;
            }
            // now loop thru assertedDeviceList
            for (const auto& assertedDevice : assertedDeviceList)
            {
                // IsEvent() needs device
                candidate.device = assertedDevice.device;
                if (eventDetectionPtr->IsEvent(candidate, eventValue))
                {
                    auto event = candidate;
                    event.trigger = assertedDevice.trigger;
                    event.accessor = assertedDevice.accessor;
                    event.setDeviceIndexTuple(assertedDevice.deviceIndexTuple);
                    std::stringstream ss;
                    ss << "Throw out an eventHdlrMgr. device: " << event.device
                       << " event: '" << event.event << "'"
                       << " deviceIndex: " << assertedDevice.deviceIndexTuple;
                    logs_err("%s\n", ss.str().c_str());
                    eventDetectionPtr->RunEventHandlers(event);
                    logs_dbg(
                        "Adding event %s to internal map with afflicted device %s\n",
                        candidate.event.c_str(), candidate.device.c_str());

                    auto eventKey =
                        candidate.event + candidate.getMainDeviceType();
                    if (eventsDetected.count(eventKey) == 0)
                    {
                        eventsDetected.insert(
                            std::pair<std::string, std::vector<std::string>>(
                                eventKey,
                                std::vector<std::string>{candidate.device}));
                    }
                    else
                    {
                        eventsDetected.at(eventKey).push_back(candidate.device);
                    }
                }
            }
        }
    } // end for (auto& pc : propertiesChanged)
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
        // key is checked just for the first object from the range (if exists)
        auto key = dbusInfo.begin()->second.at(0) + ":" + interface;
        if (map.count(key) == 0)
        {
            log_dbg("subscribing object:interface : %s\n", key.c_str());
            for (const auto& object : dbusInfo.begin()->second)
            {
                handlerList.push_back(dbus::registerServicePropertyChanged(
                    conn, object, interface, genericHandler));
            }
            map[key] = 1; // global map, indicate it is already subscribed
        }
        else
        {
            log_dbg("object:interface already subscribed: %s\n", key.c_str());
        }
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
