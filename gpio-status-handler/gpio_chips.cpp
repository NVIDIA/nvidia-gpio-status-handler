#include <gpio_chips.hpp>
#include <gpio_utils.hpp>

using phosphor::logging::entry;
using phosphor::logging::level;
using phosphor::logging::log;

using namespace std;
using json = nlohmann::json;

namespace gpio_handler
{

GpioChips::GpioChips(const GpioJsonConfig& jsonConfig)
{
    int lastErrno = 0;
    if (!openGpioChips(jsonConfig.getConfig(), lastErrno))
    {
        throw std::system_error(
            std::error_code(lastErrno, std::system_category()),
            "Failed to open all the required gpio chips");
    }
}

GpioChips::~GpioChips()
{
    closeGpioChips();
}

const map<string, gpiod_chip_t*>& GpioChips::getDbusPropMapChipObj() const
{
    return dbusPropMapChipObj;
}

// Call 'gpiod_chip_close' on every value of the 'chips' map. Clear the 'chips'
// map.
void GpioChips::closeGpioChips() noexcept
{
    for (auto it = dbusPropMapChipObj.cbegin(); it != dbusPropMapChipObj.cend();
         ++it)
    {
        gpiod_chip_t* chip = it->second;
        {
            stringstream ss;
            ss << "Closing chip '" << gpiod_chip_name(chip) << "' "
               << "(opened by '" << it->first << "')" << endl;
            log<level::INFO>(ss.str().c_str(), chipEntry(gpiod_chip_name(chip)),
                             pinNameEntry(it->first));
        }
        gpiod_chip_close(chip);
    }
    dbusPropMapChipObj.clear();
}

// Return 'true' if and only if all gpio chips specified in
// 'jsonConfig' were opened successfully. If 'false' then the
// resulting map 'dbusPropMapChipObj' is always empty.
// If result is 'true' then all keys in 'jsonConfig' are present in
// 'dbusPropMapChipObj' as well, and only them.

// A specific gpio line on the given chip cannot be requested by
// means of 'gpiod_line_request_both_edges_events' or the like
// more than once. Doing so results in an error. It doesn't mean,
// however, that a different process requested this line before -
// it could have been during the execution of 'openGpioLines' in
// the case user specified the same pin on the same chip for two
// different DBus properties in the configuration file,
// intentionally or by mistake.

// This is not allowed to happen, but to detect such case we need
// unique specification of the gpio pin and its chip. While the
// former is a unique number, the specification of a chip with a
// path is not unambiguous ("/dev/gpiochip0" =
// "/dev/../dev/gpiochip0"). It's therefore expected to be
// specified by number only, and the "/dev/gpiochip" prefix is
// being added by the program itself.

bool GpioChips::openGpioChips(const json& jsonConfig, int& lastErrno) noexcept
{
    // assert(dbusPropMapChipObj.empty()); // (1)
    bool allChipsOpenable = true;
    for (auto it = jsonConfig.cbegin();
         it != jsonConfig.cend() && allChipsOpenable; ++it)
    {
        string pinName = it.key();
        unsigned gpioChipNum = it.value()[GpioJsonConfig::configKeyGpioChip];
        // The call to 'gpiod_chip_open_by_number', if
        // successful, always results in the allocation of new
        // object (source: lib source). (101)

        {
            stringstream ss;
            ss << "Opening chip " << gpioChipNum << " (by '" << pinName << "')"
               << endl;
            log<level::INFO>(ss.str().c_str(), pinNameEntry(pinName));
        }
        gpiod_chip_t* gpioChip = gpiod_chip_open_by_number(gpioChipNum);
        if (gpioChip != NULL)
        {
            {
                stringstream ss;
                ss << "Chip '" << gpiod_chip_name(gpioChip) << "' opened";
                log<level::INFO>(ss.str().c_str(),
                                 chipEntry(gpiod_chip_name(gpioChip)),
                                 pinNameEntry(pinName));
            }
            // assert(!dbusPropMapChipObj.contains(pinName));
            // ^ Satisfied by (1) and keys uniqueness in 'jsonConfig'
            dbusPropMapChipObj[pinName] = gpioChip;
        }
        else // ! gpioChip
        {
            lastErrno = errno;
            stringstream funcall;
            funcall << "gpiod_chip_open_by_number(" << gpioChipNum << ")";
            logLibgpioCallError(funcall, (int)NULL, lastErrno);
            allChipsOpenable = false;
        }
    }
    if (!allChipsOpenable)
    {
        // Close the chips opened so far;
        closeGpioChips();
    }
    return allChipsOpenable;
}

} // namespace gpio_handler
