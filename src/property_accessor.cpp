/*
 Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.

 NVIDIA CORPORATION and its licensors retain all intellectual property
 and proprietary rights in and to this software, related documentation
 and any modifications thereto.  Any use, reproduction, disclosure or
 distribution of this software and related documentation without an express
 license agreement from NVIDIA CORPORATION is strictly prohibited.
*
*/

#include "property_accessor.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <regex>
#include <string>

/*
 * defines used in std::stdoull
 */
#define HEXA_BASE 16
#define DECIMAL_BASE 10

namespace data_accessor
{

PropertyString::PropertyString(const std::string& value) : PropertyValue()
{
    _data.strValue = value;
}

PropertyString::PropertyString(const PropertyVariant& varVar) : PropertyValue()
{
    if (std::holds_alternative<std::string>(varVar) == true)
    {
        _data.strValue = std::get<std::string>(varVar);
        _data.valid = false;
    }
    else
    {
        getPropertyDataFromVariant(varVar);
        _data.valid = false; // make sure it will not be used as Integer
    }
}

PropertyValue::PropertyValue(const std::string& value)
{
    // string2Uint64 will set all fields from _data sructure
    string2Uint64(value);
}

PropertyValue::PropertyValue(const std::string& valueStr, uint64_t value64)
{
    _data.strValue = valueStr;
    _data.value64 = value64;
    _data.valid = true;
}

PropertyValue::~PropertyValue()
{
    // Empty
}

PropertyValue::PropertyValue()
{
    // Empty
}

PropertyValue::PropertyValue(const PropertyVariant& value)
{
    getPropertyDataFromVariant(value);
}

bool PropertyValue::check(const CheckDefinitionMap& map,
                          const PropertyVariant& redefCriteria) const
{
    bool ret = false;
    for (auto& accessorCheck : map)
    {
        auto& key = accessorCheck.first;
        if (key == bitmaskKey)
        {
            ret = bitmask(criteria::getValueFromCriteria(redefCriteria,
                                                         accessorCheck.second));
        }
        else if (key == lookupKey)
        {
            ret = lookup(criteria::getStringFromCriteria(redefCriteria,
                                                         accessorCheck.second));
        }
        else if (key == equalKey)
        {
            auto value = criteria::getStringFromCriteria(redefCriteria,
                                                         accessorCheck.second);
            ret = value._data.strValue == _data.strValue;
        }
        if (ret == false)
        {
            // perhaps there will be more pairs in the future
            break;
        }
    }
    return ret;
}

bool PropertyValue::bitmask(const uint64_t mask) const
{
    return isValid() == true && (_data.value64 & mask) == mask;
}

bool PropertyValue::bitmask(const PropertyValue& otherMask) const
{
    return otherMask.isValid() && bitmask(otherMask._data.value64);
}

bool PropertyValue::lookup(const std::string& lookupString,
                           CaseSensitivity cs) const
{
    bool ret = false;
    if (cs == caseInsensitive)
    {
        std::string lookUpp = lookupString;
        std::transform(lookUpp.begin(), lookUpp.end(), lookUpp.begin(),
                       ::toupper);
        std::string selfUpp = _data.strValue;
        std::transform(selfUpp.begin(), selfUpp.end(), selfUpp.begin(),
                       ::toupper);
        ret = selfUpp.find(lookUpp) != std::string::npos;
    }
    else
    {
        ret = _data.strValue.find(lookupString) != std::string::npos;
    }
    return ret;
}

bool PropertyValue::lookup(const PropertyValue& otherLookup,
                           CaseSensitivity cs) const
{
    return lookup(otherLookup._data.strValue, cs);
}

void PropertyValue::getPropertyDataFromVariant(const PropertyVariant& varVar)
{
    bool done = false;
    if (varVar.index() != 0) // != std::monostate, has a valid value
    {
        done =
            PropertyValueDataHelper<std::string>::setString(varVar, &_data) ||
            PropertyValueDataHelper<uint8_t>::setInteger(varVar, &_data) ||
            PropertyValueDataHelper<int16_t>::setInteger(varVar, &_data) ||
            PropertyValueDataHelper<uint16_t>::setInteger(varVar, &_data) ||
            PropertyValueDataHelper<int32_t>::setInteger(varVar, &_data) ||
            PropertyValueDataHelper<uint32_t>::setInteger(varVar, &_data) ||
            PropertyValueDataHelper<int64_t>::setInteger(varVar, &_data) ||
            PropertyValueDataHelper<uint64_t>::setInteger(varVar, &_data) ||
            PropertyValueDataHelper<double>::setInteger(varVar, &_data) ||
            PropertyValueDataHelper<bool>::setBoolean(varVar, &_data);
    }
    if (done == false) // varVar.index() == 0 or type not handled above
    {
        _data = PropertyValueData(); // clear
    }
}

void PropertyValue::string2Uint64(const std::string& value)
{
    _data.strValue = value;
    _data.valid = false;
    _data.value64 = 0;
    if (value.empty() == true || value.find_first_of(' ') != std::string::npos)
    {
        return;
    }
    // first check for strings "true" and "false"
    bool strTrue = (value == "true" || value == "True" || value == "TRUE");
    bool strFalse = (value == "false" || value == "False" || value == "FALSE");
    if (strTrue == true || strFalse == true)
    {
        _data.value64 = strTrue == true ? 1 : 0; // 1 if value is one of Trues
        _data.valid = true;
    }
    else // any decimal or hexa value
    {
        uint64_t data = 0;
        if (value.at(0) == '0' && value.size() > 2 &&
            (value.at(1) == 'x' || value.at(1) == 'X'))
        {
            // Hexa first
            try
            {
                data = std::stoll(value, nullptr, HEXA_BASE);
                _data.valid = true;
                _data.value64 = data;
            }
            catch (...)
            {
                _data.valid = false;
            }
        }
        else
        {
            try
            {
                data = std::stoll(value, nullptr, DECIMAL_BASE);
                _data.valid = true;
                _data.value64 = data;
            }
            catch (...)
            {
                _data.valid = false;
            }
        }
    }
}

namespace criteria
{

PropertyString getStringFromCriteria(const PropertyVariant& redefCriteria,
                                     const std::string& accessorValue)
{
    // index() != 0 means not std::monostate
    PropertyString value = redefCriteria.index() != 0
                               ? PropertyString(redefCriteria)
                               : PropertyString(accessorValue);

    return value;
}

PropertyValue getValueFromCriteria(const PropertyVariant& redefCriteria,
                                   const std::string& accessorValue)
{
    // index() != 0 means not std::monostate
    PropertyValue value = redefCriteria.index() != 0
                              ? PropertyValue(redefCriteria)
                              : PropertyValue(accessorValue);

    return value;
}

} // namespace criteria

} // namespace data_accessor
