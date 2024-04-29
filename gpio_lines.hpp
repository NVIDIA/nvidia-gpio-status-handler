#pragma once

#include <gpiod.h>

#include <gpio_chips.hpp>
#include <gpio_json_config.hpp>
#include <gpio_status_handler.hpp>

#include <map>
#include <string>

namespace gpio_handler
{

/** @brief RAII manager of multiple opened gpio lines **/
class GpioLines
{
  public:
    /**
     * @brief Open all the gpio lines listed under @ref
     * GpioJsonConfig::configKeyGpioPin property ("gpio_pin") in the @jsonConfig
     * configuration.
     *
     * By 'opened' it's meant that both @gpiod_chip_get_line and
     * @gpiod_line_request_both_edges_events function calls from gpiod library
     * succeeded. The pin name needed for @gpiod_line_request_both_edges_events
     * is taken from the top-level attribute name of the @jsonConfig object. The
     * same name is used to obtain the corresponding gpio device handler from
     * @gpioChips.
     *
     * Throw @ref std::system_error if not all lines requested in @jsonConfig
     * could be opened. Strong exception guarantee (the state of the program is
     * rolled back to the state just before the constructor call).
     *
     * It's assumed that @gpioChips object stays alive for the
     * whole lifetime of this object.
     */
    GpioLines(const GpioChips& gpioChips, const GpioJsonConfig& jsonConfig);
    ~GpioLines();

    /**
     * @brief Get the mapping from pin names (root attributes in the @jsonConfig
     * passed to the constructor) to the opened gpio lines handlers represented
     * by @gpiod_line_t structs from the gpiod library.
     */
    const std::map<std::string, gpiod_line_t*>& getDbusPropMapLineObj() const;

  private:
    std::map<std::string, gpiod_line_t*> dbusPropMapLineObj;

    bool openGpioLines(
        const std::map<std::string, gpiod_chip_t*>& dbusPropMapChipObj,
        const nlohmann::json& jsonConfig, int& lastErrno) noexcept;
    void closeGpioLines() noexcept;
    bool requestBothEdgesEvents(int& lastErrno) noexcept;
};

} // namespace gpio_handler
