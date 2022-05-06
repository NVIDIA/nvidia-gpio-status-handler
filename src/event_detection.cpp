
/*
 *
 */
#include "event_detection.hpp"

#include "aml.hpp"
#include "aml_main.hpp"
#include "data_accessor.hpp"
#include "log.hpp"

#include <boost/container/flat_map.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <iostream>
#include <regex>

using json = nlohmann::json;

using namespace std;

namespace event_detection
{

constexpr auto dbusServiceList = {
    "xyz.openbmc_project.GpuMgr",
    "xyz.openbmc_project.GpioStatusHandler",
    "xyz.openbmc_project.Tsm",
};

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

#ifdef ENABLE_LOGS
    std::cout << "objectPath:" << objectPath << "\n";
    std::cout << "signalSignature:" << signalSignature << "\n";
    std::cout << "msgInterface:" << msgInterface << "\n";
#endif

    if (propertiesChanged.empty())
    {
#ifdef ENABLE_LOGS
        std::cout << "empty propertiesChanged, return.\n";
#endif
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
#ifdef ENABLE_LOGS
            std::cout << "empty eventProperty, skip.\n";
#endif
            continue;
        }

#ifdef ENABLE_LOGS
        cout << "Path: " << objectPath << " Property: " << eventProperty
             << ", Variant: " << variant << "\n";
#endif

        const std::string type = "DBUS";
        nlohmann::json j;
        j[data_accessor::typeKey] = type;
        j[data_accessor::accessorTypeKeys[type][0]] = objectPath;
        j[data_accessor::accessorTypeKeys[type][1]] = msgInterface;
        j[data_accessor::accessorTypeKeys[type][2]] = eventProperty;
        data_accessor::DataAccessor accessor(j);
        auto candidate = eventDetectionPtr->LookupEventFrom(accessor);
        if (candidate == nullptr)
        {
#ifdef ENABLE_LOGS
            std::cout << "No event found in the supporting list.\n";
#endif
            continue;
        }
        auto assertedDeviceNames = accessor.getAssertedDeviceNames();
        if (assertedDeviceNames.empty() == true)
        {
            // just use deviceId 0
            assertedDeviceNames[0] =
                DetermineDeviceName(objectPath, candidate->deviceType);
        }
        event_info::EventNode event = *candidate;
        int eventValue = invalidIntParam;
        if (candidate->valueAsCount)
        {
#ifdef ENABLE_LOGS
            std::cout << "event value for event " << candidate->event << ": "
                      << *variant << "\n";
#endif
            eventValue = int(*variant);
        }
        for (const auto& device : assertedDeviceNames)
        {
            event.device = device.second;
#ifdef ENABLE_LOGS
            std::cout << __func__ << "device: " << event.device
                      << " Throw out an eventHdlrMgr.\n";
#endif
            if (eventDetectionPtr->IsEvent(*candidate, eventValue))
            {
                eventDetectionPtr->RunEventHandlers(event);
            }
        }
    }
}

DbusEventHandlerList EventDetection::startEventDetection(
    EventDetection* eventDetection,
    std::shared_ptr<sdbusplus::asio::connection> conn)
{
    // it will be used in EventDetection::dbusEventHandlerCallback()
    eventDetectionPtr = eventDetection;
    auto genericHandler = std::bind(&EventDetection::dbusEventHandlerCallback,
                                    std::placeholders::_1);
    DbusEventHandlerList handlerList;
    for (auto& service : dbusServiceList)
    {
        handlerList.push_back(
            data_accessor::dbus::registerServicePropertyChanged(
                conn, service, genericHandler));
    }
#ifdef ENABLE_LOGS
    std::cout << "dbusEventHandlerMatcher created.\n";
#endif
    return handlerList;
}

#if 0
void EventDetection::identifyEventCandidate(const std::string& objPath,
                                            const std::string& signature,
                                            const std::string& property)
{
#ifdef ENABLE_LOGS
    cout << "dbus([" << objPath << "]/[" << signature << "]/[" << property
         << "]).\n";
#endif
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
#ifdef ENABLE_LOGS
                    cout << "Matched event: " << objPath << " " << eventPath
                         << "\n";
#endif
                    goto exit;
                }
            }
            else
            {
#ifdef ENABLE_LOGS
                cout << "Regex Issue";
#endif
                goto exit;
            }
        }
        std::cerr << "\n";
    }
exit:

#ifdef ENABLE_LOGS
    cout << "Exit"
         << "\n";
#endif
}
#endif
// void EventDetection::~EventDetection()
// {
// }

} // namespace event_detection
