#include <gpio_utils.hpp>
#include <phosphor-logging/log.hpp>

#include <sstream>

using namespace std;
using phosphor::logging::entry;
using phosphor::logging::level;
using phosphor::logging::log;

void logLibgpioCallError(const stringstream& funcall, int result, int lastErrno)
{
    log<level::ERR>("libgpiod function call error",
                    entry("FUNCALL=%s", funcall.str().c_str()),
                    entry("RESULT=%d", result), entry("ERRNO=%d", lastErrno),
                    entry("ERRNO_STR=%s", strerror(lastErrno)));
}

void logLibgpioCallError(const stringstream& funcall, int result, int lastErrno,
                         const string& pinName, const string& chipName,
                         int pinNum)
{
    log<level::ERR>("libgpiod function call error",
                    entry("FUNCALL=%s", funcall.str().c_str()),
                    entry("RESULT=%d", result), entry("ERRNO=%d", lastErrno),
                    entry("ERRNO_STR=%s", strerror(lastErrno)),
                    pinNameEntry(pinName), chipEntry(chipName),
                    pinNumEntry(pinNum));
}

tuple<const char*, const char*> pinNameEntry(const string& pinName)
{
    return entry("GPIO_PIN_NAME=%s", pinName.c_str());
}

tuple<const char*, const char*> chipEntry(const string& chipName)
{
    return entry("GPIO_CHIP=%s", chipName.c_str());
}

tuple<const char*, int> pinNumEntry(int pinNum)
{
    return entry("GPIO_PIN_NUM=%d", pinNum);
}
