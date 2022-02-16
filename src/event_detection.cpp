
/*
 *
 */
#include "event_detection.hpp"
#include "aml_main.hpp"
#include "aml.hpp"
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
    std::shared_ptr<sdbusplus::asio::connection> conn,
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface)
{
    auto eventHandlerMatcherCallback = [this](sdbusplus::message::message& msg) {
        std::string msgInterface;
        boost::container::flat_map<std::string, std::variant<std::string>>
            propertiesChanged;
        msg.read(msgInterface, propertiesChanged);

        std::string sensorPath = msg.get_path();

        std::string signalSignature = msg.get_signature();

        if (propertiesChanged.empty())
        {
            return;
        }

        for (auto& pc : propertiesChanged)
        {
            auto variant = std::get_if<std::string>(&pc.second);
            std::string eventProperty = pc.first;

            if (eventProperty.empty() || nullptr == variant)
            {
                continue;
            }
            // cout << "[G]" << *variant << ":" << event << "\n";

            this->identifyEventCandidate(sensorPath, signalSignature, eventProperty);

            cout << "Path: " << sensorPath << " Property: " << eventProperty << ", Variant: " << variant << "\n";
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
            sdbusplus::bus::match::rules::sender("xyz.openbmc_project.GpuMgr") +
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

void EventDetection::identifyEventCandidate(const std::string& objPath,
                                            const std::string& signature,
                                            const std::string& property)
{
    cout << "dbus([" << objPath <<
        "]/[" << signature << "]/[" << property << "]).\n";
    std::regex rgx("obj=\"(.+?)\"");
    std::smatch match;

    for (const auto& dev : event_detection::EventDetection::eventMap)
    {
        //std::cerr << dev.first << " events:\n";

        for (const auto& event : dev.second)
        {
            //std::cerr << event.event << "\n";
            if (std::regex_search(event.accessorStruct.accessorMetaData.begin(), event.accessorStruct.accessorMetaData.end(), match, rgx))
            {
                std::string eventPath = match[1];

                if (objPath.find(eventPath) != std::string::npos) {
                    cout << "Matched event: " << objPath << " " << eventPath << "\n";
                    goto exit;
                }
            }
            else
            {
                cout << "Regex Issue";
                goto exit;
            }
        }
        std::cerr << "\n";
    }
exit:

    cout << "Exit" << "\n";

}

// void EventDetection::~EventDetection()
// {
// }

} // namespace event_detection
