
/*
 *
 */
#include "event_detection.hpp"

#include "aml.hpp"
#include "aml_main.hpp"
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

std::unique_ptr<sdbusplus::bus::match_t> EventDetection::startEventDetection(
    EventDetection* evtDet, std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto dbusEventHandlerMatcherCallback =
        [evtDet](sdbusplus::message::message& msg) {
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
                auto candidate = evtDet->LookupEventFrom(accessor);
                if (candidate == nullptr)
                {
#ifdef ENABLE_LOGS
                    std::cout << "No event found in the supporting list.\n";
#endif
                    continue;
                }

                int eventValue = invalidIntParam;
                if (candidate->valueAsCount)
                {
#ifdef ENABLE_LOGS
                    std::cout << "event value for event " << candidate->event
                              << ": " << *variant << "\n";
#endif
                    eventValue = int(*variant);
                }

                if (evtDet->IsEvent(*candidate, eventValue))
                {
                    event_info::EventNode event = *candidate;
                    event.device =
                        DetermineDeviceName(objectPath, event.deviceType);

#ifdef ENABLE_LOGS
                    std::cout << "Throw out a eventHdlrMgr.\n";
#endif
                    evtDet->RunEventHandlers(event);
                }
            }

            return;
        };

    auto dbusEventHandlerMatcher = std::make_unique<sdbusplus::bus::match_t>(
        static_cast<sdbusplus::bus::bus&>(*conn),
        sdbusplus::bus::match::rules::type::signal() +
            sdbusplus::bus::match::rules::sender("xyz.openbmc_project.GpuMgr") +
            sdbusplus::bus::match::rules::member("PropertiesChanged") +
            sdbusplus::bus::match::rules::interface(
                "org.freedesktop.DBus.Properties"),
        std::move(dbusEventHandlerMatcherCallback));
#ifdef ENABLE_LOGS
    std::cout << "dbusEventHandlerMatcher created.\n";
#endif

    return dbusEventHandlerMatcher;
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
