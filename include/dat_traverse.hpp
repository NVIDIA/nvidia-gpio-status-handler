
/*
 *
 */

#pragma once

#include "aml.hpp"
#include "data_accessor.hpp"
#include "event_handler.hpp"
#include "event_info.hpp"

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace dat_traverse
{

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

    /** @brief Prints out DAT structure to verify population
     *
     * @param[in]  m  - DAT map
     *
     */
    static void printTree(const std::map<std::string, dat_traverse::Device>& m);

    /** @brief Populates DAT map with JSON profile contents
     *
     * @param[in]  m     - DAT map
     * @param[in]  file  - name of JSON file
     *
     */
    static void populateMap(std::map<std::string, dat_traverse::Device>& m,
                            const std::string& file);

    Device(const std::string& s);
    Device(const std::string& s, const json& j);
    ~Device();
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
        return aml::RcCode::succ;
    }

  private:
    // TODO: define Device Association Tree pointer here
};

} // namespace event_handler
