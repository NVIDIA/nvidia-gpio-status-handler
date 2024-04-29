#include <phosphor-logging/log.hpp>

#include <sstream>
#include <string>

/**
 * @file
 *
 * A collection of loosely coupled utilities. Currently a collection of
 * functions supporting coherent phosphor-logging library usage.
 *
 * All the output of the program goes to 'systemd-journald'. The complete
 * set of custom tags for the structured messages is:
 *
 * - FILE :: relevant to the json parsing,
 *
 * - FUNCALL, RESULT, ERRNO, ERRNO_STR :: used in any erroneous 'libgpiod'
 * function call,
 *
 * - GPIO_CHIP, GPIO_PIN_NAME, GPIO_PIN_NUM :: used in any operation on the
 * line, successful or not. Their values correspond directly to the config
 * entry:
 *
 *   - GPIO_CHIP <=> entry's "gpio_chip" property with the "gpiochip" prefix (ie
 * '0' <-> "gpiochip0"),
 *
 *   - GPIO_PIN_NAME <=> entry's key (ie "I2C3_PIN" <=> "I2C3_PIN"),
 *
 *   - GPIO_PIN_NUM <=> entry's "gpio_pin" property (ie '108' <=> '108'),
 *
 * - EXCEPTION :: universal.
 *
 * The only message levels used are INFO, WARNING and ERR. The last one
 * always precedes the termination of the program, and the internally
 * caused termination of the program should always be accompanied by ERR
 * entry. If you observed a different behavior please let me know.
 */

void logLibgpioCallError(const std::stringstream& funcall, int result,
                         int lastErrno);

void logLibgpioCallError(const std::stringstream& funcall, int result,
                         int lastErrno, const std::string& pinName,
                         const std::string& chipName, int pinNum);

std::tuple<const char*, const char*> pinNameEntry(const std::string& pinName);
std::tuple<const char*, const char*> chipEntry(const std::string& chipName);
std::tuple<const char*, int> pinNumEntry(int pinNum);

template <phosphor::logging::level L>
void logPinOperation(const char* message, const std::string& pinName,
                     const std::string& chipName, int pinNum)
{
    phosphor::logging::log<L>(message, pinNameEntry(pinName),
                              chipEntry(chipName), pinNumEntry(pinNum));
}

template <phosphor::logging::level L>
void logPinOperation(const char* message, const std::string& pinName)
{
    phosphor::logging::log<L>(message, pinNameEntry(pinName));
}
