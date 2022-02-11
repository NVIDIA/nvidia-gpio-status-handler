
/*
 *
 */

#pragma once

#include "aml.hpp"
#include "data_accessor.hpp"
#include "event_info.hpp"
#include "event_handler.hpp"

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace dat_traverse
{

class Device
{
  public:
    /* name of device */
    std::string name;

    /* downstream devices (children) */
    std::vector<std::string> association;

    /* upstream devices */
    std::vector<std::string> parents;

    /* 6-layer accessor info for this device */
    std::map<std::string, std::vector<data_accessor::DataAccessor*>> status;

    /* prints out in memory tree to verify population went as expected */
    static void printTree(const std::map<std::string, dat_traverse::Device>& m);

    /* populates memory structure with dat json contents */
    static void populateMap(std::map<std::string, dat_traverse::Device>& m,
                            const json& j);

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
