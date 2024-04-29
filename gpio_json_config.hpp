#pragma once

#include <nlohmann/json.hpp>

namespace gpio_handler
{

/**
 * @brief Represents the correctly formed configuration file
 *
 * While the 'nlohmann::json' library performs the syntactic check this class
 * focuses on the semantics. Malformed file will cause the constructor to throw
 * an exception, so that receiving the object of this type can guarantees a
 * properly formed json config.
 *
 * Properly formed config file example:
 *
 * {
 *   "I2C3_ALERT" : {
 *     "gpio_chip" : 0,
 *     "gpio_pin" : 108,
 *     "initial" : false,
 *     "read_period_sec" : 0.5
 *   },
 *   "I2C4_ALERT" : {
 *     "gpio_chip" : 0,
 *     "gpio_pin" : 109,
 *     "initial" : false,
 *     "read_period_sec" : 4
 *   }
 * }
 */
class GpioJsonConfig
{

  public:
    /** @brief Name of the property in a gpio pin configuration entry specifying
     * the gpio chip **/
    static const std::string configKeyGpioChip;
    /** @brief Name of the property in a gpio pin configuration
     * entry specifying the gpio pin **/
    static const std::string configKeyGpioPin;
    /** @brief Name of the property in a gpio pin configuration entry specifying
     * the initial value of the corresponding DBus property before the real gpio
     * pin state could be obtained **/
    static const std::string configKeyInitialPinVal;
    /** @brief Name of the property in a gpio pin configuration entry specifying
     * the minimal DBus property refresh rate. **/
    static const std::string configKeyReadPeriod;

    /** @brief Examplar config file for the help message purposes **/
    static const std::string expectedJsonConfigFormat;

    /**
     * @brief Create the json configureation object from the given @fileName.
     *
     * Specifically: 1. open the @fileName, 2. read into @nlohmann::json object,
     * 3. check the configuration format, 4. save the @nlohmann::json
     * object for future use, 5. close the file.
     *
     * At first 3 steps an exception can be thrown: 1. from the 'std'
     * library, 2. from the 'nlohmann::json' library, 3. an instance of
     * @GpioConfigError.
     */
    explicit GpioJsonConfig(const std::string& fileName);

    /** @brief Get the json config object read from @fileName
     * passed to the constructor. **/
    const nlohmann::json& getConfig() const;

  private:
    nlohmann::json config;
};

/**
 * @brief Represent the semantic error in json configuration file
 */
class GpioConfigError : public nlohmann::json::exception
{
  public:
    explicit GpioConfigError(const char* message);
};

} // namespace gpio_handler
