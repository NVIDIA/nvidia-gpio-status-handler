
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

    /** @brief 6-layer accessor info for this event **/
    std::map<std::string, std::vector<data_accessor::DataAccessor*>> status;

    /** @brief Struct containing health properties of device **/
    Status healthStatus;

    /** @brief Map of test layers  for device **/
    std::map<std::string, TestLayer> test;

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
            std::cout << this->getName() << " error: empty device\n";
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
     * @brief Fully traverses a device and stops if comparator
     *        detects an issue in which case action function
     *        will update originOfCondition
     *
     * @param dat
     * @param device
     * @param comparator
     * @param action
     *
     * @return vector of devices which we saw an issue with
     */
    std::vector<std::string> childTraverse(
        std::map<std::string, dat_traverse::Device>& dat,
        const std::string& device,
        const std::function<bool(const dat_traverse::Device& device)>
            comparator,
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
