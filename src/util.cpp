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
#include <utility>
#include <unordered_map>

namespace util
{
/**
 * With GPU naming changes, 'deviceId' also changed, it is NOT a sequence [1-8]
 */
struct MatchDevice
{
    std::string name;
    int deviceId;
};

std::unordered_map<std::string, struct MatchDevice> deviceNameMap =
{
   {"ERoT_GPU_SXM_1", {"ERoT_GPU4", 4}},
   {"ERoT_GPU_SXM_2", {"ERoT_GPU5", 5}},
   {"ERoT_GPU_SXM_3", {"ERoT_GPU6", 6}},
   {"ERoT_GPU_SXM_4", {"ERoT_GPU7", 7}},
   {"ERoT_GPU_SXM_5", {"ERoT_GPU0", 0}},
   {"ERoT_GPU_SXM_6", {"ERoT_GPU1", 1}},
   {"ERoT_GPU_SXM_7", {"ERoT_GPU2", 2}},
   {"ERoT_GPU_SXM_8", {"ERoT_GPU3", 3}},

   {"GPU_SXM_1", {"GPU4", 4}},
   {"GPU_SXM_2", {"GPU5", 5}},
   {"GPU_SXM_3", {"GPU6", 6}},
   {"GPU_SXM_4", {"GPU7", 7}},
   {"GPU_SXM_5", {"GPU0", 0}},
   {"GPU_SXM_6", {"GPU1", 1}},
   {"GPU_SXM_7", {"GPU2", 2}},
   {"GPU_SXM_8", {"GPU3", 3}},

   {"GPU_SXM_1_DRAM_0", {"GPUDRAM4", 4}},
   {"GPU_SXM_2_DRAM_0", {"GPUDRAM5", 5}},
   {"GPU_SXM_3_DRAM_0", {"GPUDRAM6", 6}},
   {"GPU_SXM_4_DRAM_0", {"GPUDRAM7", 7}},
   {"GPU_SXM_5_DRAM_0", {"GPUDRAM0", 0}},
   {"GPU_SXM_6_DRAM_0", {"GPUDRAM1", 1}},
   {"GPU_SXM_7_DRAM_0", {"GPUDRAM2", 2}},
   {"GPU_SXM_8_DRAM_0", {"GPUDRAM3", 3}}
};

/**
 * @brief This is a regular expression for ranges
 *        Also matches empty brackets []
 */
constexpr auto RANGE_REGEX = "\\[[0-9]*-*[0-9]*\\]";

constexpr auto RANGE_REGX_STR = ".*(\\[[0-9]+\\-[0-9]+\\]).*";
const auto RangeRepeaterIndicatorLength = ::strlen(RangeRepeaterIndicator);

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

int getMappedDeviceId(const std::string& deviceName)
{
   // check if the deviceName is present on deviceNameMap,
    // if so returns the 'deviceId' from this name mapping
    if (util::deviceNameMap.count(deviceName) != 0)
    {
       auto deviceNameInfo = util::deviceNameMap.at(deviceName);
       logs_dbg("deviceName: %s mapToName: %s deviceId: %d\n",
                deviceName.c_str(), deviceNameInfo.name.c_str(),
                deviceNameInfo.deviceId);
       return deviceNameInfo.deviceId;
    }
    logs_dbg("deviceName: %s deviceId = util::InvalidDeviceId = -1\n",
             deviceName.c_str());
    return util::InvalidDeviceId;
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
        // it should start from the highiest value,
        auto counter = minMax.at(1);
        for (; counter >= minMax.at(0); --counter)
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
        if (initialValue == -1)
        {
            globalValue = value;
        }
        else
        {
            globalValue = initialValue;
        }
        int finalValue = std::stoi(values[1]);
        while (value <= finalValue)
        {
            std::string rangeValue = std::to_string(value);
            std::string original = deviceRegx;
            auto expanded = original.replace(regex_position, sizeRegxStr,
                                             rangeValue);

            auto nextRepeatIndPos = expanded.find_first_of("[");
            /** Curly brackets follow an occurrence from a previous range
             *
             *  "name[1-2]/double_()_more_[1-3]") should be expanded to:
             *      'name1/double_1_more_1', 'name1/double_1_more_2',
             *      'name1/double_1_more_3', 'name2/double_2_more_1',
             *      'name2/double_2_more_2', 'name2/double_2_more_3'
             */
            auto repIndPos = expanded.find_first_of(RangeRepeaterIndicator);
            while (repIndPos != std::string::npos)
            {
                if (nextRepeatIndPos != std::string::npos && repIndPos >
                        nextRepeatIndPos)
                {
                    break;
                }
                expanded.replace(repIndPos, 2, rangeValue);
                repIndPos = expanded.find_first_of(RangeRepeaterIndicator);
            }

            if (nextRepeatIndPos != std::string::npos)
            {
               globalValue += priv_expandRange(expanded, globalValue,
                                               deviceMap);
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
        int start=-1;
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
        // device_type sometimes has range, apply "[0-9]+" on every range
        auto deviceType = util::makeRangeForRegexSearch(devType);
        const std::regex r{".*(" + deviceType + ").*"};
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
    logs_dbg("objPath %s devType:%s Devname: %s\n.",
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

std::string revertRangeRepeated(const std::string& str, size_t pos)
{
    std::string strRegex{str};
    auto position = pos;
    if (pos == std::string::npos)
    {
       position = str.find_first_of(RangeRepeaterIndicator);
    }

    // TODO create a vector of Regular expressions for cases more than one
    std::string matchedRegex = matchedRegx(strRegex, RANGE_REGX_STR);
    while (position != std::string::npos)
    {
        // TODO use matchedRegex[indexed]
        if (matchedRegex.empty() == false)
        {
            auto nextRegRegxPosition = strRegex.find_first_of("[", position);
            while (position != std::string::npos && (
                   nextRegRegxPosition == std::string::npos ||
                       position < nextRegRegxPosition))
            {
                strRegex.replace(position, RangeRepeaterIndicatorLength,
                                 matchedRegex);
                position = strRegex.find_first_of(RangeRepeaterIndicator,
                                                 position + matchedRegex.size());
            }
        }
    }
    return strRegex;
}

std::string makeRangeForRegexSearch(const std::string& rangeStr)
{
    auto rangePosition = rangeStr.find_first_of("[");
    if (rangePosition == std::string::npos)
    {
        return rangeStr;
    }
    const std::string match{"[0-9]+"};
    std::string matchRegx{rangeStr};
    while (rangePosition != std::string::npos)
    {
        auto size = matchRegx.find_first_of("]", rangePosition) -
                rangePosition + 1;
        matchRegx.replace(rangePosition, size, match);
        rangePosition = matchRegx.find_first_of("[", rangePosition + size - 1);
    }
    auto rangeRepeaterPosition = matchRegx.find_first_of(RangeRepeaterIndicator);
    while (rangeRepeaterPosition != std::string::npos)
    {
         matchRegx.replace(rangeRepeaterPosition, RangeRepeaterIndicatorLength,
                           match);
         rangeRepeaterPosition = matchRegx.find_first_of(RangeRepeaterIndicator,
                                                        rangeRepeaterPosition);
    }
    return matchRegx;
}


void splitDeviceTypeForRegxSearch(const std::string& deviceType,
                                  std::vector<std::string>& devTypePieces)
{
    decltype(deviceType.size()) start = 0;
    decltype(start) counter = 0;
    std::regex regxUnderscore{"_[A-Za-z]+"};
    std::sregex_token_iterator noMatches;
    std::sregex_token_iterator piece(
                     deviceType.begin(), deviceType.end(), regxUnderscore, -1);
    /**  split the device type by undescore applying some criteria
               does not  split if next character is a digit nor '['
            GPU_SXM_[1-8]_DRAM_0 => "GPU", "SMX_[1-8]+", "DRAM_0"
    */
    for (; piece != noMatches; piece++)
    {
        std::string subStr = *piece;
        if (subStr.empty() == false)
        {
            std::string prevPart{""};
            counter = deviceType.find(subStr, start);
            // it is also necessary to consider parts that do not match
            if (start > 0 && counter > 0)
            {
                prevPart = deviceType.substr(start+1, counter - start -1);
                start = counter;
            }
            start += subStr.size();
            devTypePieces.push_back(makeRangeForRegexSearch(prevPart + subStr));
        }
    }
    if (devTypePieces.empty() == true)
    {
        devTypePieces.push_back(makeRangeForRegexSearch(deviceType));
    }
}

std::string determineAssertedDeviceName(const std::string& realDevice,
                                        const std::string& deviceType)
{
     std::string name{""};
     if (realDevice.empty() == false && deviceType.empty() == false)
     {
         std::vector<std::string> devTypePieces;
         splitDeviceTypeForRegxSearch(deviceType, devTypePieces);
         auto counter = devTypePieces.size();
         bool matchedFlag = false;
         while (counter--)
         {
             auto part = devTypePieces.at(counter);
             const std::regex r{".*(" + part + ").*"};
             std::smatch m;
            /* if a part from deviceType matches in device uses matched part,
             *  otherwise uses the deviceType part as always based on deviceType
             */
             if (std::regex_search(realDevice.begin(), realDevice.end(), m, r))
             {
                 devTypePieces[counter] = m[1];
                 matchedFlag = true;
             }
         }
         /**
          * If not match exists, return empty string */
         if (matchedFlag == true)
         {
            if (devTypePieces.size() == 1)
            {
               name = devTypePieces.back();
            }
            else
            {
               name = boost::join(devTypePieces, "_");
            }
         }
     }
     logs_dbg("realDevice %s deviceType:%s Devname: %s\n.",
              realDevice.c_str(), deviceType.c_str(), name.c_str());
     return name;
}

bool matchRegexString(const std::string& regstr, const std::string& str)
{
    std::string myRegStr{regstr};
    std::string myStr{str};
    auto valRangRepeatPos = regstr.find_first_of(util::RangeRepeaterIndicator);
    if (valRangRepeatPos != std::string::npos)
    {
        myRegStr = revertRangeRepeated(regstr, valRangRepeatPos);
    }
    auto otherRangeRepeatPos = str.find_first_of(util::RangeRepeaterIndicator);
    if (otherRangeRepeatPos != std::string::npos)
    {
        myStr = util::revertRangeRepeated(str, otherRangeRepeatPos);
    }
    const std::regex r{myRegStr};
    return std::regex_match(myStr, r);
}

std::regex createRegexDigitsRange(const std::string &pattern)
{
    std::string regxStr{"("};
    bool last_digit = false;
    for(auto it = pattern.cbegin(); it != pattern.cend(); ++it)
    {
        if (std::isdigit(*it) == true )
        {
            if (last_digit == false)
            {
                last_digit = true;
                regxStr += RANGE_REGEX;
            }
        }
        else
        {
            regxStr.push_back(*it);
            last_digit = false;
        }
    }
    regxStr += ")" ;
    std::regex reg;
    reg.assign(regxStr);
    return reg;
}

std::string introduceDeviceInObjectpath(const std::string& objPath,
                                        const std::string& device)
{
    auto regEx = createRegexDigitsRange(device);
    auto objPathWithoutRangeRepeated = revertRangeRepeated(objPath);
    auto ret = std::regex_replace(objPathWithoutRangeRepeated, regEx, device);
    if (ret.empty() == true)
    {
        ret = objPath;
    }
    return ret;
}

} // namespace util
