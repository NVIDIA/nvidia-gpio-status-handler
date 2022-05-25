#include <gpio_json_config.hpp>
#include <gpio_lines.hpp>
#include <gpio_utils.hpp>
#include <phosphor-logging/log.hpp>

#include <sstream>

using phosphor::logging::entry;
using phosphor::logging::level;
using phosphor::logging::log;

using namespace std;
using json = nlohmann::json;

namespace gpio_handler
{

GpioLines::GpioLines(const GpioChips& gpioChips,
                     const GpioJsonConfig& jsonConfig)
{
    int lastErrno = 0;
    if (openGpioLines(gpioChips.getDbusPropMapChipObj(), jsonConfig.getConfig(),
                      lastErrno))
    {
        if (!requestBothEdgesEvents(lastErrno))
        {
            closeGpioLines();
            throw std::system_error(
                std::error_code(lastErrno, std::system_category()),
                "Failed to request names for all the gpio lines required");
        }
    }
    else
    {
        throw std::system_error(
            std::error_code(lastErrno, std::system_category()),
            "Failed to open all the required gpio lines");
    }
}

GpioLines::~GpioLines()
{
    closeGpioLines();
}

const map<string, gpiod_line_t*>& GpioLines::getDbusPropMapLineObj() const
{
    return dbusPropMapLineObj;
}

// If result is 'true' then 'dbusPropMapLineObj' contains all the
// keys 'k' from 'dbusPropMapChipObj' and the corresponding
// values 'v' were obtained by calling 'gpiod_chip_get_line'. The
// resulting 'dbusPropMapLineObj' is bijective (102).

bool GpioLines::openGpioLines(
    const map<string, gpiod_chip_t*>& dbusPropMapChipObj,
    const json& jsonConfig, int& lastErrno) noexcept
{
    // assert(dbusPropMapLineObj.empty()); // (1)

    // (gpio chip number, pin number) -> DBus property name
    map<pair<unsigned, unsigned>, string> gpioLineIds;

    bool allLinesOpenable = true;
    for (auto it = jsonConfig.cbegin();
         it != jsonConfig.cend() && allLinesOpenable; ++it)
    {
        string pinName = it.key();

        if (!dbusPropMapChipObj.contains(pinName))
        {
            allLinesOpenable = false;
        }
        else // ! !dbusPropMapChipObj.contains(pinName)
        {
            // assert(dbusPropMapChipObj.contains(pinName));
            // ^ satisfied by 'openGpioChips'
            gpiod_chip_t* chip = dbusPropMapChipObj.at(pinName);

            unsigned gpioChipNum =
                it.value()[GpioJsonConfig::configKeyGpioChip];
            unsigned pinNum = it.value()[GpioJsonConfig::configKeyGpioPin];

            pair<unsigned, unsigned> p(gpioChipNum, pinNum);
            if (!gpioLineIds.contains(p))
            {
                // In general 'gpiod_chip_get_line' may or may not
                // result in the allocation of memory. For the
                // different 'chip' object, however, the memory is
                // always allocated (source: lib source). So together
                // with (101) this call will always return a new
                // object which satisfies (102).
#ifdef ENABLE_GSH_LOGS
                {
                    stringstream ss;
                    string chipName = gpiod_chip_name(chip);
                    ss << "Opening line <" << chipName << " " << pinNum << ">"
                       << " (by '" << pinName << "')" << endl;
                    logPinOperation<level::INFO>(ss.str().c_str(), pinName,
                                                 chipName, pinNum);
                }
#endif
                gpiod_line_t* line = gpiod_chip_get_line(chip, pinNum);
                if (line != NULL)
                {
                    // assert(!dbusPropMapLineObj.contains(pinName));
                    // ^ Satisfied by (1) and keys uniqueness in 'jsonConfig'
                    dbusPropMapLineObj[pinName] = line;
                    gpioLineIds[p] = pinName;
                }
                else // ! line
                {
                    lastErrno = errno;
                    string chipName = gpiod_chip_name(chip);
                    stringstream ss;
                    ss << "gpiod_chip_get_line(\"" << chipName << "\", "
                       << pinNum << ")";
                    logLibgpioCallError(ss, (int)NULL, lastErrno, pinName,
                                        chipName, pinNum);
                    allLinesOpenable = false;
                }
            }
            else // ! !gpioLineIds.contains(p)
            {
                stringstream ss;
                string chipName = gpiod_chip_name(chip);
                ss << "Pin number " << pinNum << " on the gpio chip '"
                   << chipName << "' associated with the DBus property '"
                   << pinName
                   << "' has already been associated with the property '"
                   << gpioLineIds[p] << "'";
                log<level::ERR>(ss.str().c_str(), pinNameEntry(pinName),
                                chipEntry(chipName), pinNumEntry(pinNum));
                allLinesOpenable = false;
            }
        }
    }
    if (!allLinesOpenable)
    {
        closeGpioLines();
    }
    return allLinesOpenable;
}

void GpioLines::closeGpioLines() noexcept
{
    for (auto it = dbusPropMapLineObj.cbegin(); it != dbusPropMapLineObj.cend();
         ++it)
    {
#ifdef ENABLE_GSH_LOGS
        {
            stringstream ss;
            ss << "Closing gpio line requested by '" << it->first << "'";
            logPinOperation<level::INFO>(ss.str().c_str(), it->first);
        }
#endif
        gpiod_line_release(it->second);
    }
    dbusPropMapLineObj.clear();
}

// If result is 'false' then at least one line among the values of
// 'dbusPropMapLineObj' could not be requested because it was requested by
// another process.
// If result is 'true' then for every value 'v' in 'dbusPropMapLineObj' the
// 'gpiod_line_is_requested(v)' is also true, and 'v' can be used in
// 'gpiod_line_*' methods in this process.

bool GpioLines::requestBothEdgesEvents(int& lastErrno) noexcept
{
    // assert(dbusPropMapLineObj is bijective) - satisfied by (102)
    bool allLinesRequestable = true;
    for (auto it = dbusPropMapLineObj.cbegin();
         it != dbusPropMapLineObj.cend() && allLinesRequestable; ++it)
    {
        string pinName = it->first;
        gpiod_line_t* line = it->second;
#ifdef ENABLE_GSH_LOGS
        {
            stringstream ss;
            int pinNum = gpiod_line_offset(line);
            string chipName = gpiod_chip_name(gpiod_line_get_chip(line));
            ss << "Requesting line <" << chipName << " " << pinNum
               << "> using name '" << pinName << "'";
            logPinOperation<level::INFO>(ss.str().c_str(), pinName, chipName,
                                         pinNum);
        }
#endif
        int requestResult =
            gpiod_line_request_both_edges_events(line, pinName.c_str());
        if (requestResult != 0)
        {
            lastErrno = errno;
            stringstream ss;
            int pinNum = gpiod_line_offset(line);
            string chipName = gpiod_chip_name(gpiod_line_get_chip(line));
            ss << "gpiod_line_request_both_edges_events(<" << chipName << " "
               << pinNum << ">, \"" << pinName << "\")";
            logLibgpioCallError(ss, requestResult, lastErrno, pinName, chipName,
                                pinNum);
            allLinesRequestable = false;
        }
    }
    return allLinesRequestable;
}

} // namespace gpio_handler
