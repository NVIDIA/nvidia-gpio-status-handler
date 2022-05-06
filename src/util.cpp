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

DeviceIdMap expandDeviceRange(const std::string& deviceRegx)
{
    DeviceIdMap deviceMap;
    if (deviceRegx.empty() == true)
    {
        return deviceMap; // return empty map
    }
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
        int finalValue = std::stoi(values[1]);
        while (value <= finalValue)
        {
            std::string rangeValue = std::to_string(value);
            std::string original = deviceRegx;
            deviceMap[value] =
                original.replace(regex_position, sizeRegxStr, rangeValue);
            value++;
        }
    }
    return deviceMap;
}

std::string replaceRangeByMatchedValue(const std::string& regxValue,
                                       const std::string& matchedValue)
{
    std::string newString{regxValue};
    if (matchedValue.empty() == false)
    {
        auto info = getMinMaxRange(regxValue);
        auto minMax = std::get<std::vector<int>>(info);
        if (minMax.size() == 2)
        {
            auto counter = minMax.at(0);
            for (; counter <= minMax.at(1); ++counter)
            {
                auto rangeDigit = std::to_string(counter);
                if (matchedValue.find(rangeDigit) != std::string::npos)
                {
                    auto rangeStr = std::get<std::string>(info);
                    auto rangePosition = regxValue.find(rangeStr);
                    newString.replace(rangePosition, rangeStr.size(),
                                      matchedValue);
                    break;
                }
            }
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

} // namespace util
