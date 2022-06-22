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
#include <string>
#include <tuple>
#include <vector>
#include <variant>

/**
 * It is a common type used in openbmc DBus services
 */
using Association = std::tuple<std::string, std::string, std::string>;

/**
 *     This strange type just replaces std::monostate which should be a class
 *     and due that it is not supported by sdbusplus functions that requires
 *     std::variant as parameter
 *
 *     It is not expected to use a variable of this type
 **/
using InvalidMonoState =
         std::map<uint16_t, std::map<uint16_t, std::map<uint16_t, uint16_t>>>;

/**
 *  Variant type used for Dbus blocking 'get' and 'set' properties
 */
using PropertyVariant =
    std::variant<InvalidMonoState, bool, uint8_t, int16_t, uint16_t,
                 int32_t, uint32_t, int64_t, uint64_t, double, std::string,
                 std::vector<std::string>, std::vector<Association>>;

/**
 * @brief returns true if the PropertyVariant has a valid value
 * @param variant
 * @return
 */
inline bool isValidVariant(const PropertyVariant& variant)
{
    return variant.index() != 0;
}

/**
 * @brief returns true if the PropertyVariant has an invalid value
 * @param variant
 * @return
 */
inline bool isInvalidVariant(const PropertyVariant& variant)
{
    return variant.index() == 0;
}
