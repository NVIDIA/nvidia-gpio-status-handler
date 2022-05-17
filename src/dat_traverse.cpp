
#include "dat_traverse.hpp"

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
    std::map<std::string, dat_traverse::Device>& dat, const std::string& device)
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
        const dat_traverse::Device& node = dat.at(deviceName);

        for (const auto& child : node.association)
        {
            fringe.push(child);
            childVec.push_back(child);
        }
    }

    return childVec;
}

std::vector<std::string>
    DATTraverse::getAssociationConnectedDevices(const std::string& rootDevice)
{
    auto allVisited = childTraverse(
        dat, rootDevice,
        []([[maybe_unused]] const auto& ignored) { return true; }, {});
    // Omit the first element which is always 'rootDevice'
    return std::vector<std::string>(allVisited.begin() + 1, allVisited.end());
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
 * @brief Query 'xyz.openbmc_project.ObjectMapper' for the name of the service
 * managing the given device.
 *
 * Equivalent of
 *
 *   busctl call                                     \
 *     xyz.openbmc_project.ObjectMapper              \
 *     /xyz/openbmc_project/object_mapper            \
 *     xyz.openbmc_project.ObjectMapper              \
 *     GetObject sas                                 \
 *     @devicePath                                   \
 *     1 xyz.openbmc_project.Association.Definitions
 *
 * It's assumed the call above returns an array of length no greater than 1
 * (that is, the service implementing 'Association.Definitions' interface is
 * unambiguous. In case more services are given it's unspecified which one will
 * be returned.)
 *
 * @param[in] devicePath
 *
 * @return Name of the service analogous (or equal) to
 * 'xyz.openbmc_project.GpuMgr'
 */

std::string dbusGetManagerServiceName(const std::string& devicePath)
{
    using namespace sdbusplus;
    using namespace std;
    auto theBus = bus::new_default_system();
    auto method =
        theBus.new_method_call("xyz.openbmc_project.ObjectMapper",
                               "/xyz/openbmc_project/object_mapper",
                               "xyz.openbmc_project.ObjectMapper", "GetObject");

    string assocIntf = "xyz.openbmc_project.Association.Definitions";

    method.append(devicePath);
    vector<string> interfaces = {assocIntf};
    method.append(interfaces);

    auto reply = theBus.call(method);

    map<string, vector<string>> dbusResult;
    reply.read(dbusResult);

    string result;
    if (reply.is_method_error())
    {
        cout << "Error in dbusGetManagerServiceName property" << endl;
        return "";
    }
    else if (dbusResult.empty())
    {
        cout << "No manager associated with the device '" << devicePath << "'"
             << endl;
        return "";
    }
    else
    {
        return dbusResult.begin()->first;
    }
}

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
    auto theBus = bus::new_default_system();
    auto method =
        theBus.new_method_call(manager.c_str(), devicePath.c_str(),
                               "org.freedesktop.DBus.Properties", "Get");
    method.append("xyz.openbmc_project.Association.Definitions");
    method.append("Associations");
    auto reply = theBus.call(method);
    std::variant<PropertyType> dbusResult;
    reply.read(dbusResult);
    if (reply.is_method_error())
    {
        std::cout << "Error in dbusGetDeviceAssociations" << std::endl;
        return {};
    }
    else
    {
        return std::get<PropertyType>(dbusResult);
    }
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
    auto method =
        theBus.new_method_call(manager.c_str(), devicePath.c_str(),
                               "org.freedesktop.DBus.Properties", "Set");
    method.append("xyz.openbmc_project.Association.Definitions");
    method.append("Associations");
    variant<vector<tuple<string, string, string>>> variantValues = values;
    method.append(variantValues);
    auto reply = theBus.call(method);
    // TODO:
    if (reply.is_method_error())
    {
        cout << "Error in dbusSetDeviceAssociations" << endl;
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

void dbusAddHealthRollupAssociations(
    const std::string& manager, const std::string& devicePath,
    const std::vector<std::string>& subAssocDevicePaths)
{
    using namespace std;
    PropertyType values = dbusGetDeviceAssociations(manager, devicePath);
    for (const auto& devicePath : subAssocDevicePaths)
    {
        tuple<string, string, string> newAssoc("health_rollup", "", devicePath);
        values.push_back(newAssoc);
    }
    dbusSetDeviceAssociations(manager, devicePath, values);
}

/**
 * @brief Get the list of device object paths for future
 * "health_rollup" setting.
 *
 * Get the list of device object paths which are sub-objects of
 * '/xyz/openbmc_project/inventory/system/chassis/' and implement
 * 'xyz.openbmc_project.Association.Definitions' interface.
 *
 * The equivalent of calling
 *
 *   busctl call                                      \
 *     xyz.openbmc_project.ObjectMapper               \
 *     /xyz/openbmc_project/object_mapper             \
 *     xyz.openbmc_project.ObjectMapper               \
 *     GetSubTreePaths sias                           \
 *     /xyz/openbmc_project/inventory/system/chassis/ \
 *     0                                              \
 *     1 xyz.openbmc_project.Association.Definitions
 *
 * @return Something along
 *
 *   [
 *     ...
 *     "/xyz/openbmc_project/inventory/system/chassis/GPU5"
 *     "/xyz/openbmc_project/inventory/system/chassis/GPU6"
 *     "/xyz/openbmc_project/inventory/system/chassis/GPU7"
 *     "/xyz/openbmc_project/inventory/system/chassis/NVSwitch0"
 *     "/xyz/openbmc_project/inventory/system/chassis/NVSwitch1"
 *     ...
 *   ]
 *
 * or an empty vector if error in method call occured.
 */

std::vector<std::string> dbusGetDeviceObjectPaths()
{

    using namespace sdbusplus;
    auto theBus = bus::new_default_system();

    auto method = theBus.new_method_call("xyz.openbmc_project.ObjectMapper",
                                         "/xyz/openbmc_project/object_mapper",
                                         "xyz.openbmc_project.ObjectMapper",
                                         "GetSubTreePaths");

    std::string assocIntf = "xyz.openbmc_project.Association.Definitions";
    std::string rootObject = "/xyz/openbmc_project/inventory/system/chassis/";

    method.append(rootObject);
    method.append(0);
    std::vector<std::string> interfaces = {assocIntf};
    method.append(interfaces);

    auto reply = theBus.call(method);

    std::vector<std::string> result;
    reply.read(result);

    if (reply.is_method_error())
    {
        std::cout << "Error in setPcapEnabled property" << std::endl;
        return {};
    }
    else
    {
        return result;
    }
}

/**
 * https://stackoverflow.com/questions/236129/how-do-i-iterate-over-the-words-of-a-string
 */
template <typename Out>
void split(const std::string& s, char delim, Out result)
{
    std::istringstream iss(s);
    std::string item;
    while (std::getline(iss, item, delim))
    {
        *result++ = item;
    }
}

std::vector<std::string> split(const std::string& s, char delim)
{
    std::vector<std::string> elems;
    split(s, delim, std::back_inserter(elems));
    return elems;
}

/**
 * Given
 *
 *   [
 *     ...
 *     "/xyz/openbmc_project/inventory/system/chassis/GPU5"
 *     "/xyz/openbmc_project/inventory/system/chassis/GPU6"
 *     "/xyz/openbmc_project/inventory/system/chassis/GPU7"
 *     "/xyz/openbmc_project/inventory/system/chassis/NVSwitch0"
 *     "/xyz/openbmc_project/inventory/system/chassis/NVSwitch1"
 *     ...
 *   ]
 *
 * convert it to
 *
 *   [
 *     ...
 *     "GPU5"      -> "/xyz/openbmc_project/inventory/system/chassis/GPU5"
 *     "GPU6"      -> "/xyz/openbmc_project/inventory/system/chassis/GPU6"
 *     "GPU7"      -> "/xyz/openbmc_project/inventory/system/chassis/GPU7"
 *     "NVSwitch0" -> "/xyz/openbmc_project/inventory/system/chassis/NVSwitch0"
 *     "NVSwitch1" -> "/xyz/openbmc_project/inventory/system/chassis/NVSwitch1"
 *     ...
 *   ]
 *
 * @param[in] deviceObjectPaths
 */

std::map<std::string, std::string> createDeviceNameMapDeviceObjectPath(
    const std::vector<std::string>& deviceObjectPaths)
{
    std::map<std::string, std::string> result;
    for (const auto& objectPath : deviceObjectPaths)
    {
        auto elems = split(objectPath, '/');
        auto lastElem = elems.back();
        result[lastElem] = objectPath;
    }
    return result;
}

/**
 * Limits of the current implementation
 *
 * Note 1: Service 'xyz.openbmc_project.ObjectMapper' is assumed to be ready and
 * populated with all devices.
 *
 * Note 2: Function doesn't check for duplicates for now. When executed multiple
 * times (eg. restarted oobaml) the 'Associations' property of the devices will
 * be filled with redundant data.
 *
 * Note 3: It's assumed '/xyz/openbmc_project/object_mapper' returns exactly one
 * manager for every given device. If no managers are returned the code will
 * fail, if more than one - it's unspecified which one will be used.
 *
 * Note 4: No error logging to systemd is being done currently.
 *
 */
void DATTraverse::datToDbusAssociation()
{
    using namespace std;

    auto devNameMapDevObjPath =
        createDeviceNameMapDeviceObjectPath(dbusGetDeviceObjectPaths());

    for (const auto& [devName, dev] : dat)
    {
        if (devNameMapDevObjPath.contains(devName))
        {
            std::string devObjPath = devNameMapDevObjPath.at(devName);
            auto associations = this->getAssociationConnectedDevices(devName);

            // Remove the devices which don't occur in 'devNameMapDevObjPath'
            // map, that is which aren't modeled in the ObjectMapper.
            std::erase_if(associations,
                          [&devNameMapDevObjPath](const std::string& elem) {
                              return !devNameMapDevObjPath.contains(elem);
                          });

            std::transform(associations.begin(), associations.end(),
                           associations.begin(),
                           [&devNameMapDevObjPath](const std::string& elem) {
                               return devNameMapDevObjPath.at(elem);
                           });

            std::string manager = dbusGetManagerServiceName(devObjPath);
            dbusAddHealthRollupAssociations(manager, devObjPath, associations);
        }
    }
}

} // namespace event_handler
