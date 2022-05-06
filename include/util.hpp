/*
 Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.

 NVIDIA CORPORATION and its licensors retain all intellectual property
 and proprietary rights in and to this software, related documentation
 and any modifications thereto.  Any use, reproduction, disclosure or
 distribution of this software and related documentation without an express
 license agreement from NVIDIA CORPORATION is strictly prohibited.
*
*/

#pragma once

#include <map>
#include <regex>
#include <string>
#include <tuple>

namespace util
{

using DeviceIdMap = std::map<int, std::string>;

void printThreadId(const char* funcName);

/**
 * @brief  performs std::regex_search(str, rgx)
 *
 * @param str  common string
 *
 * @param rgx  regular expression
 *
 * @return the part of the str which matches or an empty string (no matches)
 */
std::string matchedRegx(const std::string& str, const std::string& rgx);

bool existsRegx(const std::string& str, const std::string& rgx);

bool existsRange(const std::string& str);

std::tuple<std::vector<int>, std::string>
    getMinMaxRange(const std::string& rgx);

/**
 * @brief checks if name is not empty and it is not regular expression
 * @param name   an object path such as /xyz/path/deviceName
 * @return  the last part of a valid object path
 */
std::string getDeviceName(const std::string& name);

/**
 *  @brief  returns the device Id of a device
 *
 *  @param   [optional] range
 *
 *  @example getDeviceId("GPU5") should return 5
 *           getDeviceId("GPU6-ERoT") should return 6
 *           getDeviceId("GPU0") should return 0
 *           getDeviceId("PCIeSwitch") should return 0
 *           getDeviceId"GPU6-ERoT", "GPU[0-7]-ERoT") should return 6
 *           getDeviceId("GPU9", "GPU[0-7]") should return -1
 *
 *  @return the ID when found otherwise (-1 if range is specified or 0 if not)
 */
int getDeviceId(const std::string& deviceName,
                const std::string& range = std::string{""});

/**
 * @brief   Expands range in a string and returns a list of expanded strings
 *
 * Calls example:
 * @example
 *   ("[0-5]");          '0'  '1'  '2'  '3'  '4'  '5'
 *   ("name[1-4]");      'name1'  'name2'  'name3'  'name4'
 *   (begin[0-2]end");   'begin0end'  'begin1end'  'begin2end'
 *   ("unique");         'unique'
 *   ("[2-3]end");       '2end'  '3end'
 *
 * @return DeviceIdMap which contains both deviceId and deviceName
 */
DeviceIdMap expandDeviceRange(const std::string& deviceRegx);

/**
 * @brief replaceRangeByMatchedValue
 * @param value
 * @return a replaced string or regxValue in case matchedValue does not match
 */
std::string replaceRangeByMatchedValue(const std::string& regxValue,
                                       const std::string& matchedValue);

} // namespace util
