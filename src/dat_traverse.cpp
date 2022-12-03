
#include "dat_traverse.hpp"

#include "dbus_accessor.hpp"

#include <nlohmann/json.hpp>
#include <sdbusplus/bus.hpp>

#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <ostream>
#include <queue>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

using json = nlohmann::json;

namespace dat_traverse
{

Device::~Device()
{}

void Device::populateMap(std::map<std::string, dat_traverse::Device>& dat,
                         const std::string& file)
{
    std::ifstream i(file);
    json j;
    i >> j;

    for (const auto& el : j.items())
    {
        auto deviceName = el.key();
        dat_traverse::Device device(deviceName, el.value());
        dat.insert(
            std::pair<std::string, dat_traverse::Device>(deviceName, device));
    }

    // fill out parents of all child devices on 2nd pass
    for (const auto& entry : dat)
    {
        for (const auto& child : entry.second.association)
        {
            if (dat.count(child) > 0)
            {
                dat.at(child).parents.push_back(entry.first);
            }
            else
            {
                std::cerr << "Error: deviceName:" << child
                          << "does not exist in DAT!" << std::endl;
            }
        }
    }
}

std::ostream& operator<<(std::ostream& os, const Device& device)
{
    os << "\tname:\t" << device.name << "\n";

    os << "\tchildren:";
    for (auto& assoc : device.association)
    {
        os << "\t" << assoc;
    }

    os << "\n\tparents:";
    for (auto& par : device.parents)
    {
        os << "\t" << par;
    }

    os << "\n\thealth status:";
    os << "\t[" << device.healthStatus.health;
    os << "]\t[" << device.healthStatus.healthRollup;
    os << "]\t[" << device.healthStatus.originOfCondition;
    os << "]\t[" << device.healthStatus.triState << "]\n";

    for (auto& layer : device.test)
    {
        os << "\ttest layer [" << layer.first << "]\n";
        for (auto& tp : layer.second.testPoints)
        {
            os << "\t\tTP [" << tp.first << "]";
            os << "\t" << tp.second.accessor;
            os << "\t" << tp.second.expectedValue << "\n";
        }
    }

    return os;
}

void Device::printTree(const std::map<std::string, dat_traverse::Device>& m)
{
    std::queue<std::string> fringe;
    fringe.push("Baseboard");
    while (!fringe.empty())
    {
        std::string deviceName = fringe.front();
        fringe.pop();

        if (m.count(deviceName) == 0)
        {
            std::cerr << "Error: deviceName:" << deviceName
                      << "is an invalid key!" << std::endl;
            continue;
        }

        dat_traverse::Device device = m.at(deviceName);

        std::cerr << "Found Device " << deviceName << "\n";
        for (const auto& parent : device.parents)
        {
            std::cerr << parent << " is a parent of " << deviceName << "\n";
        }

        for (const auto& child : device.association)
        {
            std::cerr << "Found child " << child << " of device " << deviceName
                      << "\n";
            fringe.push(child);
        }
        std::cerr << "\n";
    }
}

Device::Device(const std::string& name)
{
    this->name = name;
}

Device::Device(const std::string& name, const json& j)
{

    this->name = name;
    this->association = j["association"].get<std::vector<std::string>>();
    this->healthStatus.health = "OK";
    this->healthStatus.healthRollup = "OK";
    this->healthStatus.originOfCondition = "";
    this->healthStatus.triState = "Active";

    std::list<std::string> layers = {"power_rail",      "erot_control",
                                     "pin_status",      "interface_status",
                                     "protocol_status", "firmware_status"};
    /* add optional data_dump field */
    if (j.count("data_dump") > 0)
    {
        layers.push_back("data_dump");
    }

    std::map<std::string, dat_traverse::TestLayer> test;

    for (const auto& layer : layers)
    {
        std::map<std::string, dat_traverse::TestPoint> testPoints;
        for (const auto& point : j[layer])
        {
            dat_traverse::TestPoint tp;
            tp.accessor = point["accessor"];
            tp.expectedValue = (point.count("expected_value") == 0)
                                   ? ""
                                   : point["expected_value"].get<std::string>();
            std::string name = point["name"].get<std::string>();
            testPoints.insert(
                std::pair<std::string, dat_traverse::TestPoint>(name, tp));
        }

        dat_traverse::TestLayer testLayer;
        testLayer.testPoints = testPoints;
        test.insert(
            std::pair<std::string, dat_traverse::TestLayer>(layer, testLayer));
    }
    this->test = test;
}

} // namespace dat_traverse

namespace event_handler
{

DATTraverse::~DATTraverse()
{}

void DATTraverse::printBranch(
    const std::map<std::string, dat_traverse::Device>& dat,
    const std::vector<std::string>& devices)
{
    for (const auto& dev : devices)
    {
        std::cerr << dev << ":\n";
        if (dat.count(dev) == 0)
        {
            std::cerr << "Error: deviceName:" << dev
                      << "does not exist in DAT. Skipping print." << std::endl;
            continue;
        }
        auto device = dat.at(dev);
        std::cerr << "Health: " << device.healthStatus.health << "\n";
        std::cerr << "Healthrollup: " << device.healthStatus.healthRollup
                  << "\n";
        std::cerr << "OOC: " << device.healthStatus.originOfCondition << "\n";
        std::cerr << "State: " << device.healthStatus.triState << "\n\n";
    }
}

void DATTraverse::setDAT(const std::map<std::string, dat_traverse::Device>& dat)
{
    this->dat = dat;
}

bool DATTraverse::hasParents(const dat_traverse::Device& device)
{
    return device.parents.size() > 0;
}

bool DATTraverse::checkHealth(const dat_traverse::Device& device)
{
    return device.healthStatus.health == std::string("OK");
}

void DATTraverse::setHealthProperties(dat_traverse::Device& targetDevice,
                                      const dat_traverse::Status& status)
{
    targetDevice.healthStatus.healthRollup = status.healthRollup;
    targetDevice.healthStatus.triState = status.triState;
}

void DATTraverse::setOriginOfCondition(dat_traverse::Device& targetDevice,
                                       const dat_traverse::Status& status)
{
    targetDevice.healthStatus.originOfCondition = status.originOfCondition;
}

std::vector<std::string> DATTraverse::getSubAssociations(
    std::map<std::string, dat_traverse::Device>& dat, const std::string& device,
    const bool doTraverseTestpoints)
{
    std::queue<std::string> fringe;
    std::vector<std::string> childVec;
    fringe.push(device);
    childVec.push_back(device);

    while (!fringe.empty())
    {
        std::string deviceName = fringe.front();
        fringe.pop();

        if (dat.count(deviceName) == 0)
        {
            std::cerr << "Error: deviceName:" << deviceName
                      << "is an invalid key!" << std::endl;
            continue;
        }

        if (doTraverseTestpoints)
        {
            dat_traverse::Device& node = dat.at(deviceName);
            for (auto& layer : node.test)
            {
                for (auto& tp : layer.second.testPoints)
                {
                    if (tp.second.accessor.isValidDeviceAccessor())
                    {
                        auto child = tp.second.accessor.read();
                        fringe.push(child);
                        childVec.push_back(child);
                    }
                }
            }
        }
        else
        {
            const dat_traverse::Device& node = dat.at(deviceName);
            for (const auto& child : node.association)
            {
                fringe.push(child);
                childVec.push_back(child);
            }
        }
    }

    return childVec;
}

std::vector<std::string>
    DATTraverse::getAssociationConnectedDevices(const std::string& rootDevice)
{
    return childTraverse(
        dat, rootDevice,
        []([[maybe_unused]] const auto& ignored) { return true; }, {});
}

std::vector<std::string> DATTraverse::childTraverse(
    std::map<std::string, dat_traverse::Device>& dat, const std::string& device,
    const std::function<bool(const dat_traverse::Device& device)> predicate,
    const std::vector<
        std::function<void(std::map<std::string, dat_traverse::Device>& dat,
                           const dat_traverse::Device& device)>>& action)
{
    std::vector<std::string> result;
    // Invariant: 'toVisit' has no common elements with 'result'
    std::vector<std::string> toVisit;
    toVisit.push_back(device);
    while (!toVisit.empty())
    {
        std::string currentDeviceName = toVisit.back();
        toVisit.pop_back();
        if (std::find(result.cbegin(), result.cend(), currentDeviceName) ==
            result.cend())
        {
            // Object 'assocDev' not found in 'result'
            result.push_back(currentDeviceName);
            const auto& currentDevice = dat.at(currentDeviceName);
            for (const auto& callback : action)
            {
                callback(dat, currentDevice);
            }
            if (predicate(currentDevice))
            {
                for (const auto& assocDev : currentDevice.association)
                {
                    toVisit.push_back(assocDev);
                }
            }
        }
    }
    return result;
}

void DATTraverse::parentTraverse(
    std::map<std::string, dat_traverse::Device>& dat, const std::string& device,
    const std::function<bool(const dat_traverse::Device& device)> comparator,
    const std::vector<std::function<void(dat_traverse::Device& device,
                                         const dat_traverse::Status& status)>>&
        action)
{
    if (dat.count(device) == 0)
    {
        std::cerr << "Error: deviceName:" << device << "is an invalid key!"
                  << std::endl;
        return;
    }

    if (dat.count(device) == 0)
    {
        std::cerr << __PRETTY_FUNCTION__ << " device:" << device
                  << " not found in dat.json" << std::endl;
        return;
    }
    dat_traverse::Device& dev = dat.at(device);

    dat_traverse::Status status;
    status.health = dev.healthStatus.health;
    status.healthRollup = dev.healthStatus.health;
    status.originOfCondition = device;
    status.triState = dev.healthStatus.triState;

    std::queue<std::string> fringe;
    fringe.push(device);

    while (!fringe.empty())
    {
        std::string deviceName = fringe.front();
        fringe.pop();

        if (dat.count(deviceName) == 0)
        {
            std::cerr << "Error: deviceName:" << deviceName
                      << "is an invalid key!" << std::endl;
            continue;
        }

        dat_traverse::Device& node = dat.at(deviceName);

        for (const auto& callback : action)
        {
            callback(node, status);
        }

        if (comparator(node))
        {
            for (const auto& parent : node.parents)
            {
                fringe.push(parent);
            }
        }
    }
}

using PropertyType =
    std::vector<std::tuple<std::string, std::string, std::string>>;

/**
 * @brief Get the 'Associations' property of the @devicePath object
 *
 * Equivalent of
 *
 *    busctl get-property                           \
 *      @manager                                    \
 *      @devicePath                                 \
 *      xyz.openbmc_project.Association.Definitions \
 *      Associations
 *
 * @param[in] manager
 * @param[in] devicePath
 *
 * @return Something along the
 *
 *   [
 *     ("all_processors",  "parent_chassis",
 *         "/xyz/openbmc_project/inventory/system/processors/GPU0"),
 *     ("all_memory",      "parent_chassis",
 *         "/xyz/openbmc_project/inventory/system/memory/GPUDRAM0"),
 *     ("all_sensors",     "chassis",
 *         "/xyz/openbmc_project/sensors/temperature/TEMP_GB_GPU0"),
 *     ("all_sensors",     "chassis",
 *         "/xyz/openbmc_project/sensors/temperature/TEMP_GB_GPU0_M"),
 *     ("all_sensors",     "chassis",
 *         "/xyz/openbmc_project/sensors/power/PWR_GB_GPU0"),
 *     ("all_sensors",     "chassis",
 *         "/xyz/openbmc_project/sensors/energy/EG_GB_GPU0")
 *     ...
 *   ]
 *
 * or empty vector if error in method call occured.
 */
std::vector<std::tuple<std::string, std::string, std::string>>
    dbusGetDeviceAssociations(const std::string& manager,
                              const std::string& devicePath)
{

    using namespace sdbusplus;
    std::variant<PropertyType> dbusResult;
    auto theBus = bus::new_default_system();
    try
    {
        auto method =
            theBus.new_method_call(manager.c_str(), devicePath.c_str(),
                                   "org.freedesktop.DBus.Properties", "Get");
        method.append("xyz.openbmc_project.Association.Definitions");
        method.append("Associations");
        auto reply = theBus.call(method);
        reply.read(dbusResult);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        logs_err(" Dbus Error: %s\n", e.what());
        throw std::runtime_error(e.what());
    }
    return std::get<PropertyType>(dbusResult);
}

/**
 * @brief Set the 'Associations' property of the @devicePath object
 *
 * Equivalent of
 *
 *    busctl set-property                           \
 *      @manager                                    \
 *      @devicePath                                 \
 *      xyz.openbmc_project.Association.Definitions \
 *      Associations
 *
 *
 * @param[in] manager
 * @param[in] devicePath
 * @param[in] values
 */

void dbusSetDeviceAssociations(
    const std::string& manager, const std::string& devicePath,
    const std::vector<std::tuple<std::string, std::string, std::string>>&
        values)
{
    using namespace sdbusplus;
    using namespace std;
    auto theBus = bus::new_default_system();
    try
    {
        auto method =
            theBus.new_method_call(manager.c_str(), devicePath.c_str(),
                                   "org.freedesktop.DBus.Properties", "Set");
        method.append("xyz.openbmc_project.Association.Definitions");
        method.append("Associations");
        variant<vector<tuple<string, string, string>>> variantValues = values;
        method.append(variantValues);
        auto reply = theBus.call(method);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        logs_err(" Dbus Error: %s\n", e.what());
        throw std::runtime_error(e.what());
    }
}

/**
 * @brief Add sub associations resulting from DAT traversal to the
 * 'Associations' property of the @devicePath object, under the "health_rollup"
 * key.
 *
 * Having the 'Associations' property of the @devicePath object, under the
 * service @manager, in the 'xyz.openbmc_project.Association.Definitions'
 * interface, expand the, for example:
 *
 *   [
 *     ("all_processors",  "parent_chassis",
 *         "/xyz/openbmc_project/inventory/system/processors/GPU0"),
 *     ("all_memory",      "parent_chassis",
 *         "/xyz/openbmc_project/inventory/system/memory/GPUDRAM0"),
 *     ("all_sensors",     "chassis",
 *         "/xyz/openbmc_project/sensors/temperature/TEMP_GB_GPU0"),
 *     ("all_sensors",     "chassis",
 *         "/xyz/openbmc_project/sensors/temperature/TEMP_GB_GPU0_M"),
 *   ]
 *
 * to:
 *
 *   [
 *     ("all_processors",  "parent_chassis",
 *         "/xyz/openbmc_project/inventory/system/processors/GPU0"),
 *     ("all_memory",      "parent_chassis",
 *         "/xyz/openbmc_project/inventory/system/memory/GPUDRAM0"),
 *     ("all_sensors",     "chassis",
 *         "/xyz/openbmc_project/sensors/temperature/TEMP_GB_GPU0"),
 *     ("all_sensors",     "chassis",
 *         "/xyz/openbmc_project/sensors/temperature/TEMP_GB_GPU0_M"),
 *     ("health_rollup",   "",
 *         "/xyz/openbmc_project/inventory/system/chassis/GPU0"),
 *     ("health_rollup",   "",
 *         "/xyz/openbmc_project/inventory/system/chassis/NVSwitch0"),
 *   ]
 *
 * for "/xyz/.../GPU0", "/xyz/.../NVSwitch0" being the values of
 * @subAssocDevicePaths.
 *
 * @param[in] manager
 * @param[in] devicePath
 * @param[in] subAssocDevicePaths
 *
 */

void DATTraverse::dbusSetHealthRollupAssociations(
    const std::string& manager, const std::string& devicePath,
    const std::vector<std::string>& subAssocDevicePaths)
{
    using namespace std;
    PropertyType values = dbusGetDeviceAssociations(manager, devicePath);
    // Remove existing "health_rollup" associations
    std::erase_if(values, [](const tuple<string, string, string>& elem) {
        return get<0>(elem) == "health_rollup";
    });
    // Add the newly created ones
    for (const auto& devicePath : subAssocDevicePaths)
    {
        tuple<string, string, string> newAssoc("health_rollup", "", devicePath);
        values.push_back(newAssoc);
    }
    dbusSetDeviceAssociations(manager, devicePath, values);
}

void DATTraverse::datToDbusAssociation()
{
    using namespace std;
    dbus::CachingObjectMapper om;
    for (const auto& [devId, dev] : dat)
    {
        datToDbusAssociation(om, devId);
    }
}

} // namespace event_handler
