/*
 Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.

 NVIDIA CORPORATION and its licensors retain all intellectual property
 and proprietary rights in and to this software, related documentation
 and any modifications thereto.  Any use, reproduction, disclosure or
 distribution of this software and related documentation without an express
 license agreement from NVIDIA CORPORATION is strictly prohibited.
*
*/
#include "util.hpp"

#include "log.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include <iostream>
#include <thread>

namespace util
{

constexpr auto RANGE_REGX_STR = ".*(\\[[0-9]+\\-[0-9]+\\]).*";

/**
 *  this regex matches: "GPU2", "GPU5-ERoT", "NVSwitch2"
 */
constexpr auto DEVICE_RANGE_REGX = ".*([0-9]+)$|.*([0-9]+)\\-.*";

std::string matchedRegx(const std::string& str, const std::string& rgx)
{
    const std::regex reg{rgx};
    std::smatch match;
    std::string ret{""};
    if (std::regex_search(str, match, reg))
    {
        auto size = match.size();
        while (size--)
        {
            const std::string matched = match[size];
            if (matched.empty() == false)
            {
                ret = matched;
                break;
            }
        }
    }
    return ret;
}

bool existsRegx(const std::string& str, const std::string& rgx)
{
    auto matched = matchedRegx(str, rgx);
    return matched.empty() == false;
}

bool existsRange(const std::string& str)
{
    bool ret = false;
    if (str.empty() == false)
    {
        ret = existsRegx(str, RANGE_REGX_STR);
    }
    return ret;
}

std::string removeRange(const std::string& str)
{
    std::string ret{str};
    if (str.empty() == false)
    {
        std::string matched = matchedRegx(ret, RANGE_REGX_STR);
        auto pos = std::string::npos;
        if (matched.empty() == true)
        {
            matched = matchedRegx(ret, DEVICE_RANGE_REGX);
            if (matched.empty() == false)
            {
                pos = ret.find_last_of(matched);
            }
        }
        else
        {
            pos = ret.find(matched);
        }
        if (pos != std::string::npos)
        {
            ret.erase(pos, matched.size());
        }
    }
    return ret;
}

int getDeviceId(const std::string& deviceName, const std::string& range)
{
    std::string validRange{range};
    int ret = -1;
    if (range.empty() == true)
    {
        validRange = DEVICE_RANGE_REGX;
        ret = 0;
    }

    auto info = getMinMaxRange(validRange);
    auto minMax = std::get<std::vector<int>>(info);
    if (minMax.size() == 2)
    {
        auto counter = minMax.at(0);
        for (; counter <= minMax.at(1); ++counter)
        {
            auto rangeDigit = std::to_string(counter);
            if (deviceName.find(rangeDigit) != std::string::npos)
            {
                ret = counter;
                break;
            }
        }
    }
    return ret;
}

int priv_expandRange(const std::string& deviceRegx, int initialValue,
                     DeviceIdMap& deviceMap )
{
    int globalValue = 0;
    std::string matchedStr =
        (deviceRegx.find_first_of("[") == 0 &&
         deviceRegx.find_last_of("]") == deviceRegx.size() - 1)
            ? deviceRegx
            : matchedRegx(deviceRegx, RANGE_REGX_STR);

    if (matchedStr.empty() == true)
    {
        auto deviceId = getDeviceId(deviceRegx);
        deviceMap[deviceId] = deviceRegx;
    }
    else
    {
        std::string regxStr = matchedStr;
        size_t regex_position = deviceRegx.find(regxStr);
        auto sizeRegxStr = regxStr.size();
        // remote brackets
        regxStr.erase(regxStr.size() - 1, 1);
        regxStr.erase(0, 1);
        std::vector<std::string> values;
        boost::split(values, regxStr, boost::is_any_of("-"));
        int value = std::stoi(values[0]);
        globalValue = initialValue + value;
        int finalValue = std::stoi(values[1]);
        while (value <= finalValue)
        {
            std::string rangeValue = std::to_string(value);
            std::string original = deviceRegx;
            auto expanded = original.replace(regex_position, sizeRegxStr,
                                             rangeValue);

            if (expanded.find_last_of("[") != std::string::npos)
            {
               globalValue += priv_expandRange(expanded, globalValue -1,
                                               deviceMap) - 1;
            }
            else
            {
                deviceMap[globalValue] = expanded; // nothing more to expand
                globalValue++;
            }
            value++;
        }
    }
    return globalValue - initialValue;
}


DeviceIdMap expandDeviceRange(const std::string& deviceRegx)
{
    DeviceIdMap deviceMap;
    if (deviceRegx.empty() == false)
    {
        int start=0;
        priv_expandRange(deviceRegx, start, deviceMap);
    }
    return deviceMap;
}

std::string replaceRangeByMatchedValue(const std::string& regxValue,
                                       const std::string& matchedValue)
{
    std::string newString{regxValue};
    if (matchedValue.empty() == false)
    {
        auto info = getRangeInformation(regxValue);
        auto rangeSize = std::get<0>(info);
        if (rangeSize > 0)
        {

            auto rangePosition = std::get<1>(info);
            auto rangeStr = std::get<2>(info);
            newString.replace(rangePosition, rangeSize, matchedValue);
        }
    }
    return newString;
}

std::tuple<std::vector<int>, std::string> getMinMaxRange(const std::string& rgx)
{
    std::vector<int> minMax;
    std::string matchedRegex{""};

    matchedRegex =
        (rgx.find_first_of("[") == 0 && rgx.find_last_of("]") == rgx.size() - 1)
            ? rgx
            : matchedRegx(rgx, RANGE_REGX_STR);
    if (matchedRegex.empty() == false)
    {
        auto regxStr = matchedRegex;
        // remote brackets
        regxStr.erase(regxStr.size() - 1, 1);
        regxStr.erase(0, 1);
        std::vector<std::string> values;
        boost::split(values, regxStr, boost::is_any_of("-"));
        minMax.push_back(std::stoi(values[0]));
        minMax.push_back(std::stoi(values[1]));
    }
    return std::make_tuple(minMax, matchedRegex);
}

std::string getDeviceName(const std::string& name)
{
    std::string deviceName{""};
    if (name.empty() == false && existsRange(name) == false)
    {
        auto lastSlash = name.find_last_of("/");
        if (lastSlash != std::string::npos)
        {
            deviceName = name.substr(lastSlash + 1);
        }
        else
        {
            deviceName = name;
        }
    }
    return deviceName;
}

void printThreadId(const char* funcName)
{
    std::thread::id this_id = std::this_thread::get_id();
    std::cout << funcName << " thread id: " << this_id << std::endl;
}

/**
 * @brief Determine device name from DBus object path.
 *
 * @param objPath
 * @param devType
 * @return std::string
 */
std::string determineDeviceName(const std::string& objPath,
                                const std::string& devType)
{
    std::string name{objPath};
    if (objPath.empty() == false && devType.empty() == false)
    {
        // device_type sometimes has range, remove it if that exists
        auto deviceType = util::removeRange(devType);
        const std::regex r{".*(" + deviceType + "[0-9]+).*"}; // TODO: fixme
        std::smatch m;

        if (std::regex_search(objPath.begin(), objPath.end(), m, r))
        {
            name = m[1]; // the 2nd field is the matched substring.
        }
        else
        {
            // name =  expandDeviceRange(devType)[0];
        }
    }
    logs_dbg("determineDeviceName() objPath %s devType:%s Devname: %s\n.",
             objPath.c_str(), devType.c_str(), name.c_str());
    return name;
}

RangeInformation getRangeInformation(const std::string& str)
{
    SizeString sizeString{0};
    StringPosition stringPosition{std::string::npos};
    std::string fullRegxString{""};
    std::vector<int> minMax;
    std::string matchedRegex = matchedRegx(str, RANGE_REGX_STR);

    // std::regex_search does not work for str="blabla []" nor for str="[0-9]"
    if (matchedRegex.empty() == true)
    {
        auto openBracketsPos = str.find_first_of("[");
        if (openBracketsPos != std::string::npos)
        {
            auto closeBrackets = str.find_last_of("]");
            if (closeBrackets != std::string::npos)
            {
                // first case regex is the string itself, i.e, str = [0-9]
                if (openBracketsPos == 0 && closeBrackets == (str.size() - 1))
                {
                    matchedRegex = str;
                }
                else // check for empty brackets
                {
                    std::string auxRange{"["};
                    while (++openBracketsPos <= closeBrackets)
                    {
                        if (str.at(openBracketsPos) == ']')
                        {
                            auxRange.push_back(str.at(openBracketsPos));
                            // empty brackets, size = 2
                            matchedRegex = auxRange;
                            break;
                        }
                        else if (isspace(str.at(openBracketsPos)) == 0)
                        {
                            break; // not empty brackets
                        }
                    }
                }
            }
        }
    }

    if (matchedRegex.empty() == false)
    {
        auto auxPosition = str.find(matchedRegex);
        if (auxPosition != std::string::npos)
        {
            while (auxPosition-- && str.at(auxPosition) != ' ')
                ;
            stringPosition = ++auxPosition;
            while (auxPosition < str.size() && str.at(auxPosition) != ' ')
            {
                sizeString++;
                auxPosition++;
            }
            fullRegxString = str.substr(stringPosition, sizeString);
        }
    }
    return std::make_tuple(sizeString, stringPosition, fullRegxString);
}

} // namespace util
