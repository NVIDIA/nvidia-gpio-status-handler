
/*
 *
 */

#pragma once

#include "aml.hpp"
#include "data_accessor.hpp"
#include "event_handler.hpp"
#include "event_info.hpp"

#include <nlohmann/json.hpp>

#include <functional>
#include <list>
#include <map>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace dat_traverse
{

struct Status
{
    std::string health;
    std::string healthRollup;
    std::string originOfCondition;
    std::string triState;
};

struct TestPoint
{
    data_accessor::DataAccessor accessor;
    std::string expectedValue;
};

struct TestLayer
{
    std::map<std::string, TestPoint> testPoints;
};

/** @class Device
 *  @brief Object to represent Device in DAT that
 *         stores info from dat json profile
 */
class Device
{
  public:
    /** @brief Name of the device **/
    std::string name;

    /** @brief Downstream devices (children) **/
    std::vector<std::string> association;

    /** @brief Upstream devices **/
    std::vector<std::string> parents;

    /** @brief Struct containing health properties of device **/
    Status healthStatus;

    /** @brief Map of test layers  for device **/
    std::map<std::string, TestLayer> test;

    /**
     * @brief Outputs device data to verify content
     *
     * @param os
     * @param device
     * @return std::ostream&
     */
    friend std::ostream& operator<<(std::ostream& os, const Device& device);

    /** @brief Prints out DAT structure to verify population
     *
     * @param[in]  m  - DAT map
     *
     */
    static void printTree(const std::map<std::string, dat_traverse::Device>& m);

    /** @brief Populates DAT map with JSON profile contents
     *
     * @param[in]  dat     - DAT map
     * @param[in]  file    - name of JSON file
     *
     */
    static void populateMap(std::map<std::string, dat_traverse::Device>& dat,
                            const std::string& file);

    explicit Device(const std::string& s);
    Device(const std::string& s, const json& j);
    ~Device();

  private:
};

} // namespace dat_traverse

namespace event_handler
{

/**
 * @brief A class for collect device telemetries and update device status based
 * on event.
 *
 */
class DATTraverse : public event_handler::EventHandler
{
  public:
    DATTraverse(const std::string& name = __PRETTY_FUNCTION__) :
        event_handler::EventHandler(name)
    {}
    ~DATTraverse();

  public:
    /**
     * @brief Traverse the Device Association Tree for every device telemetries
     * and update device status.
     *
     * @param event
     * @return aml::RcCode
     */
    aml::RcCode process([[maybe_unused]] event_info::EventNode& event) override
    {
        std::string problemDevice = event.device;
        if (problemDevice.length() == 0)
        {
#ifdef ENABLE_LOGS
            std::cout << this->getName() << " error: empty device\n";
#endif
            return aml::RcCode::error;
        }

        std::vector<std::function<void(dat_traverse::Device & device,
                                       const dat_traverse::Status& status)>>
            parentCallbacks;
        parentCallbacks.push_back(setHealthProperties);
        parentCallbacks.push_back(setOriginOfCondition);

        parentTraverse(this->dat, problemDevice, hasParents, parentCallbacks);

        return aml::RcCode::succ;
    }

    /**
     * @brief Print health/healthrollup/OOC/state of branch specified in vector
     * @param dat
     * @param devices
     */
    void printBranch(const std::map<std::string, dat_traverse::Device>& dat,
                     const std::vector<std::string>& devices);

    /**
     * @brief Provide DAT structure so we can traverse it
     * @param dat
     */
    void setDAT(const std::map<std::string, dat_traverse::Device>& dat);

    /**
     *  @brief Return the list of all devices reachable by
     *  "association" relation.
     *
     *  @param[in] dat The device Association Tree the
     *  @rootDevice is part of.
     *
     *  @param[in] rootDevice The device from which to start the
     *  @dat traversal.
     *
     *  @return Vector of device names (keys from @dat map)
     *  reachable from @rootDevice, excluding the @rootDevice
     */
    std::vector<std::string>
        getAssociationConnectedDevices(const std::string& rootDevice);

    /**
     * @brief Populate 'Associations' property of the devices in dbus with
     * "health_rollup" entries.
     *
     * For each device 'D' found among the descendant of
     * '/xyz/openbmc_project/inventory/system/chassis/' in
     * 'xyz.openbmc_project.ObjectMapper' populate the 'Associations' property
     * of the 'xyz.openbmc_project.Association.Definitions' interface through
     * the manager service 'M' for this device (more on it later), with entries
     * of the form:
     *
     *   ("health_rollup", "", d)
     *
     * where 'd' is every device reachable from 'D' in a device association tree
     * given in @ref this->dat (excluding 'D' itself), for which there exists
     * the corresponding object path in 'xyz.openbmc_project.ObjectMapper'
     * ('d' is actually that path itself, not the mere name of the device, eg.
     * '/xyz/openbmc_project/inventory/system/processors/GPU0' instead of
     * 'GPU0').
     *
     * Obtain the managing service 'M' for the given device 'D' by querying
     * object '/xyz/openbmc_project/object_mapper' of the
     * 'xyz.openbmc_project.ObjectMapper' service using method 'GetObject' (eg.
     * 'xyz.openbmc_project.GpuMgr').
     */
    void datToDbusAssociation();

    //  private:

    /**
     * @brief searches for vector of associated subdevices
     *
     * @param dat
     * @param device
     *
     * @return vector of associated devices to device argument
     */
    static std::vector<std::string>
        getSubAssociations(std::map<std::string, dat_traverse::Device>& dat,
                           const std::string& device);

    /**
     * @brief Fully traverses a device and stops if predicate
     *        detects an issue in which case action function
     *        will update originOfCondition
     *
     * Walk Device Association Tree @dat starting at @device in a
     * depth-first manner, where the children of @device are taken
     * from its @association field. Apply every function in @action
     * vector to the @dat and currently visited device, respectively
     * (including the starting @device). Use @predicate to determine
     * whether to explore its children: @true - yes, @false - no.
     *
     * Keep track of the visited devices. @predicate will never be
     * called more than once on a device, and neither any of the
     * functions in @action.
     *
     * Return the list of names of visited devices, in the order of
     * visiting. It will always contain the @device as the first
     * element.
     *
     * @param dat
     * @param device
     * @param predicate
     * @param action
     *
     * @return vector of devices which we saw an issue with
     */
    std::vector<std::string> childTraverse(
        std::map<std::string, dat_traverse::Device>& dat,
        const std::string& device,
        const std::function<bool(const dat_traverse::Device& device)> predicate,
        const std::vector<
            std::function<void(std::map<std::string, dat_traverse::Device>& dat,
                               const dat_traverse::Device& device)>>& action);

    /**
     * @brief Fully traverses a device in reverse direction
              (from child to parent) and stops if comparator
     *        detects an issue in which case action function
     *        will update originOfCondition, health,
     *        healthrollup, etc,
     *
     * @param dat
     * @param device
     * @param comparator
     * @param action
     *
     */
    void parentTraverse(
        std::map<std::string, dat_traverse::Device>& dat,
        const std::string& device,
        const std::function<bool(const dat_traverse::Device& device)>
            comparator,
        const std::vector<std::function<void(
            dat_traverse::Device& device, const dat_traverse::Status& status)>>&
            action);

    /**
     * @brief Checks to see if the device has parents
     *
     * @param device
     * @return boolean for whether or not parent(s) were found
     */
    static bool hasParents(const dat_traverse::Device& device);

    /**
     * @brief Checks the health of the device
     *
     * @param device
     * @return boolean for whether or not device is healthy
     */
    static bool checkHealth(const dat_traverse::Device& device);

    /**
     * @brief Sets origion of condition to all upstream devices
     *
     * @param device
     * @param dat
     *
     */
    static void setOriginOfCondition(dat_traverse::Device& targetDevice,
                                     const dat_traverse::Status& status);

    /**
     * @brief Sets healthrollup/state properties
     *
     * @param device
     * @param dat
     *
     */
    static void setHealthProperties(dat_traverse::Device& targetDevice,
                                    const dat_traverse::Status& status);

    /** @brief Data structure for the DAT to traverse **/
    std::map<std::string, dat_traverse::Device> dat;

    // TODO: define Device Association Tree pointer here
};

} // namespace event_handler
