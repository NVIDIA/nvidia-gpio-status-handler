#pragma once

#include <gpiod.h>

#include <gpio_json_config.hpp>
#include <gpio_status_handler.hpp>

#include <map>
#include <string>

namespace gpio_handler
{

/** @brief RAII manager of multiple opened gpio devices **/
class GpioChips
{
  public:
    /**
     * @brief Open all the gpio devices listed under property @ref
     * GpioJsonConfig::configKeyGpioChip ("gpio_chip") in the @jsonConfig
     * configuration.
     *
     * There is exactly one 'open' operation per every entry in @jsonConfig
     * object. If a single gpio chip is associated with multiple entries it is
     * opened multiple times.
     *
     * Throw @ref std::system_error if not all chips requested in @jsonConfig
     * could be opened. Strong exception guarantee (the state of the program is
     * rolled back to the state just before the constructor call).
     */
    GpioChips(const GpioJsonConfig& jsonConfig);
    /**
     * @brief Close all the gpio devices managed by this object.
     */
    ~GpioChips();

    /**
     * @brief Get the mapping from pin names (root attributes in the @jsonConfig
     * passed to the constructor) to the opened gpio devices represented by
     * 'gpiod_chip_t' structs from the gpiod library.
     *
     * The user is responsible for not messing with the values of this map
     * (closing and such), as these operations are possible despite the 'const'.
     */
    const std::map<std::string, gpiod_chip_t*>& getDbusPropMapChipObj() const;

  private:
    std::map<std::string, gpiod_chip_t*> dbusPropMapChipObj;

    bool openGpioChips(const nlohmann::json& jsonConfig,
                       int& lastErrno) noexcept;
    void closeGpioChips() noexcept;
};

} // namespace gpio_handler
