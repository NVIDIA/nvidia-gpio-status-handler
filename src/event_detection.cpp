
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

void EventDetection::dbusEventHandlerCallback(sdbusplus::message::message& msg)
{
    std::string msgInterface;
    boost::container::flat_map<std::string, std::variant<double>>
        propertiesChanged;
    msg.read(msgInterface, propertiesChanged);

    std::string objectPath = msg.get_path();

    std::string signalSignature = msg.get_signature();

    logs_dbg("objectPath:%s\nsignalSignature:%s\nmsgInterface:%s\n",
             objectPath.c_str(), signalSignature.c_str(), msgInterface.c_str());

    if (propertiesChanged.empty())
    {
        logs_dbg("empty propertiesChanged, return.\n");
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
        auto variant = std::get_if<double>(&pc.second);
        std::string eventProperty = pc.first;

        if (eventProperty.empty() || nullptr == variant)
        {
            logs_dbg("empty eventProperty, skip.\n");
            continue;
        }

        logs_dbg("Path: %s, Property: %s, Variant: %lf\n", objectPath.c_str(),
                 eventProperty.c_str(), *variant);

        const std::string type = "DBUS";
        nlohmann::json j;
        j[data_accessor::typeKey] = type;
        j[data_accessor::accessorTypeKeys[type][0]] = objectPath;
        j[data_accessor::accessorTypeKeys[type][1]] = msgInterface;
        j[data_accessor::accessorTypeKeys[type][2]] = eventProperty;
        data_accessor::DataAccessor accessor(j);
        auto assertedEventsList = eventDetectionPtr->LookupEventFrom(accessor);
        if (assertedEventsList.empty() == true)
        {
            logs_dbg("No event found in the supporting list.\n");
            continue;
        }
        for (auto& assertedEvent : assertedEventsList)
        {
            auto& candidate = *assertedEvent.first;
            auto event = candidate;
            int eventValue = invalidIntParam;
            if (candidate.valueAsCount)
            {
                eventValue = int(*variant);
            }
            const auto& assertedDeviceNames = assertedEvent.second;
            // this is the case when the "check" operation is not 'bitmap'
            //   that means, the check does not loop over device range
            if (assertedDeviceNames.empty() == true)
            {
                std::cerr << __FILE__ << ":" << __LINE__
                          << " event: " << event.event
                          << " no assertedDeviceNames, exiting..." << std::endl;
                continue;
            }
            // now loop thru candidate.assertedDeviceNames
            for (const auto& device : assertedDeviceNames)
            {
                event.device = device.second;
                logs_dbg(
                    "%s:%d Throw out an eventHdlrMgr. device: %s event: %s",
                    __FILE__, __LINE__, event.device.c_str(),
                    event.event.c_str());
                if (eventDetectionPtr->IsEvent(candidate, eventValue))
                {
                    eventDetectionPtr->RunEventHandlers(event);
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

void
EventDetection::subscribeAcc(const data_accessor::DataAccessor& acc,
                             RegisteredObjectInterfaceMap& map,
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
