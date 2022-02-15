
/*
 *
 */
#include "event_detection.hpp"

#include <boost/container/flat_map.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <iostream>

using json = nlohmann::json;

using namespace std;

namespace event_detection
{
std::unique_ptr<sdbusplus::bus::match_t> EventDetection::startEventDetection(
    std::shared_ptr<sdbusplus::asio::connection> conn,
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface)
{
    auto eventHandlerMatcherCallback = [](sdbusplus::message::message& msg) {
        std::string msgInterface;
        boost::container::flat_map<std::string, std::variant<std::string>>
            propertiesChanged;
        msg.read(msgInterface, propertiesChanged);

        if (propertiesChanged.empty())
        {
            return;
        }

        for (auto& pc : propertiesChanged)
        {
            auto variant = std::get_if<std::string>(&pc.second);
            std::string event = pc.first;

            if (event.empty() || nullptr == variant)
            {
                continue;
            }
            // cout << "[G]" << *variant << ":" << event << "\n";

            cout << "Event: " << event << ", Variant: " << variant << "\n";
        }

        return;
    };

    // sdbusplus::bus::match_t eventHandlerMatcher(
    //     static_cast<sdbusplus::bus::bus&>(*conn),
    //     sdbusplus::bus::match::rules::type::signal() +
    //     sdbusplus::bus::match::rules::member("PropertiesChanged") +
    //     sdbusplus::bus::match::rules::interface("org.freedesktop.DBus.Properties")
    //     + sdbusplus::bus::match::rules::argN(0, "xyz.openbmc_project"),
    //     std::move(eventHandlerMatcherCallback));

    bool registerd = iface->register_signal<void>("PropertiesChanged");

    cout << "Signal-registered: " << registerd << "\n";

    // auto eventHandlerMatcher = std::make_unique<sdbusplus::bus::match_t>(
    //     static_cast<sdbusplus::bus::bus&>(*conn),
    //     sdbusplus::bus::match::rules::propertiesChangedNamespace("/xyz/openbmc_project",
    //     "xyz.openbmc_project"), std::move(eventHandlerMatcherCallback));

    auto eventHandlerMatcher = std::make_unique<sdbusplus::bus::match_t>(
        static_cast<sdbusplus::bus::bus&>(*conn),
        sdbusplus::bus::match::rules::type::signal() +
            sdbusplus::bus::match::rules::member("PropertiesChanged") +
            sdbusplus::bus::match::rules::interface(
                "org.freedesktop.DBus.Properties"),
        std::move(eventHandlerMatcherCallback));

    // sdbusplus::bus::match::match eventHandlerMatcher(
    // static_cast<sdbusplus::bus::bus&>(*conn),
    // "type='signal',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged',arg0namespace='xyz.openbmc_project.GpuMgr'",
    // std::move(eventHandlerMatcherCallback));

    return eventHandlerMatcher;
}

// void EventDetection::~EventDetection()
// {
// }

} // namespace event_detection
