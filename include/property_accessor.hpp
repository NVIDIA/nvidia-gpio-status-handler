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

#include "property_variant.hpp"

#include <map>
#include <string>

namespace data_accessor
{

using CheckDefinitionMap = std::map<std::string, std::string>;

constexpr auto bitmaskKey = "bitmask";
constexpr auto bitmapKey = "bitmap";
constexpr auto lookupKey = "lookup";
constexpr auto equalKey = "equal";
constexpr auto notEqualKey = "not_equal";

/**
 * @brief PropertyValueData represents any Data with 64 bit integer
 *
 *     Fields:
 *           strValue      string representation
 *           value64       64 bits value intended for bit operations
 *           state         State of the current Data @sa DataState
 */
struct PropertyValueData
{
    enum DataState
    {
        /** No data (nothing), nor an empty string which is data */
        Empty,
        /** The Data contains a valid Integer value even 0 */
        Integer,
        /** The data is string, the conversion to Integer was not possible or
         *   it is not desired */
        StringOnly
    };
    std::string strValue;
    uint64_t value64;
    DataState state;
    PropertyValueData() : strValue{""}, value64(0), state(Empty)
    {
        // Empty
    }
    explicit PropertyValueData(const std::string& str) :
        strValue(str), value64(0), state(StringOnly)
    {
        // Empty
    }
};

/**
 * @brief  Helper class to get values from std::variant and store that in
 *         a PropertyValueData structure
 */
template <class T>
class PropertyValueDataHelper
{
  private:
    PropertyValueDataHelper();

  public:
    /**
     * @brief converts integer 8/16 bits and double values to integer 64 bits
     *
     * @param varVar the integer as std::variant
     *
     * @param pointer of PropertyValueData to store results
     *
     * @return true if integer/double could be extracted from varVar
     */
    static bool setInteger(const PropertyVariant& varVar,
                           PropertyValueData* data)
    {
        bool isInteger = std::is_same<T, uint8_t>::value ||
                         std::is_same<T, int16_t>::value ||
                         std::is_same<T, uint16_t>::value ||
                         std::is_same<T, int32_t>::value ||
                         std::is_same<T, uint32_t>::value ||
                         std::is_same<T, int64_t>::value ||
                         std::is_same<T, uint64_t>::value ||
                         std::is_same<T, double>::value;
        if (isInteger && std::holds_alternative<T>(varVar) == true)
        {
            auto value = std::get<T>(varVar);
            data->value64 = static_cast<uint64_t>(value);
            data->strValue = std::to_string(value);
            data->state = PropertyValueData::Integer;
            return true;
        }
        return false;
    }
    /**
     * @brief setBoolean converts bool values into integer 64 bits
     *                   and sets the string representation of the data
     *
     * @param varVar the boolean value as std::variant
     *
     * @param pointer of PropertyValueData to store results
     *
     * @return true if boolean data could be extracted from varVar
     */
    static bool setBoolean(const PropertyVariant& varVar,
                           PropertyValueData* data)
    {
        if (std::holds_alternative<bool>(varVar) == true)
        {
            auto booleanData = std::get<bool>(varVar);
            data->state = PropertyValueData::Integer;
            if (booleanData == true)
            {
                data->strValue = "true";
                data->value64 = 1;
            }
            else
            {
                data->strValue = "false";
                data->value64 = 0;
            }
            return true;
        }
        return false;
    }
    /**
     * @brief  gets the string from varVar and puts it in a PropertyValueData
     *
     *
     * @param varVar the string as std::variant
     *
     * @param pointer of PropertyValueData to store results
     *
     * @return true if string data could be extracted from varVar
     */
    static bool setString(const PropertyVariant& varVar,
                          PropertyValueData* data)
    {
        if (std::holds_alternative<std::string>(varVar) == true)
        {
            data->strValue = std::get<std::string>(varVar);
            data->value64 = 0;
            data->state = PropertyValueData::StringOnly;
            return true;
        }
        return false;
    }
};

/**
 * @brief The PropertyValue intends to perform operations in between to values
 *
 *   Converts supported types to integer 64 bits and does operations such as
 *      and bit a bit among others
 */
class PropertyValue
{
  public:
    /**
     * @brief The CaseSensitivity enum
     *
     * Defines the type of string search used in lookup() methods
     */
    enum CaseSensitivity
    {
        /**
         * "AA" != "aa"
         */
        caseSensitive,

        /**
         * "AA" == "aa"
         */
        caseInsensitive
    };
    PropertyValue();
    explicit PropertyValue(const PropertyVariant& value);
    explicit PropertyValue(const std::string& value);
    explicit PropertyValue(const std::string& valueStr, uint64_t value64);
    virtual ~PropertyValue();

    /**
     * @brief operator ==  Compares two PropertyValue Data
     *
     *      This comparing returns true:
     * @code
     *         if (PropertyValue(std::string("001")) == PropertyValue(0x01))
     *         {
     *             std::cout << "They are equal";
     *         }
     * @endcode
     *
     * @param other other PropertyValue object to compare
     *
     * @return  returns true if both data have equal values even strings differ
    */
    inline bool operator==(const PropertyValue& other) const
    {
        if (this->isValidInteger() == true && other.isValidInteger() == true)
        {
            return this->getInteger() == other.getInteger();
        }
        return this->getString() == other.getString();
    }

    /**
     * @brief check() does high level operation between 2 PropertyValue objects
     *
     *
     * @param map a std::map of two strings: 'key' and its 'value'
     *
     *             key:   can be: "bitmask", "lookup" or other operation
     *             value: the value to perform the operation against this
     *
     * @return  the check result between this and the other PropertyValue object
     */
    bool check(const CheckDefinitionMap& map,
               const PropertyVariant& redefCriteria = PropertyVariant()) const;

    /**
     * @brief bitmask performs a bitmask (or bit a bit) from  the value stored
     *                in _data against the value of mask
     *
     * @param mask  the bitmask to check
     *
     * @return true if all bits set from mask are also set in the integer value
     *              from _data
     */
    bool bitmask(const uint64_t mask) const;

    /**
     * @brief bitmask performs a bitmask (or bit a bit) from  the value stored
     *                in _data against the mask stored in otherMask
     *
     * @param otherMask the PropertyValue which contais the mask
     *
     * @return true if all bits set from the integer value in otherMask
     *              are also set the integer value from _data
     */
    bool bitmask(const PropertyValue& otherMask) const;

    /**
     * @brief lookup searches lookupString in the string representation of the
     *                       _data
     *
     * @param lookupString string to search in _data
     *
     * @param cs  peforms the search case sensitive or insensitive
     *
     * @return true if lookupString was found in the string from _data
     */
    bool lookup(const std::string& lookupString,
                CaseSensitivity cs = caseSensitive) const;

    /**
     * @brief lookup searches the string otherLookup._data.strValue in the
     *               string representation of the _data
     *
     * @param otherLookup contains the string representation to search
     *
     * @param cs  peforms the search case sensitive or insensitive
     *
     * @return  true if otherLookup._data.strValue was found in the string
     *          from _data
     */
    bool lookup(const PropertyValue& otherLookup,
                CaseSensitivity cs = caseSensitive) const;

    /**
     * @brief getString
     * @return the string representation of the stored data
     */
    inline std::string getString() const
    {
        return _data.strValue;
    }

    /**
     * @brief getInteger
     * @return the 64 bits integer representation of the stored data
     */
    inline uint64_t getInteger() const
    {
        return _data.value64;
    }

    /**
     * @brief isValidInteger
     * @return true if the 64 bits integer conversion was OK even when zero
     */
    inline bool isValidInteger() const
    {
        return _data.state == PropertyValueData::Integer;
    }

    /**
     * @brief clear, just clears the data @sa empty()
     */
    inline void clear()
    {
        _data.strValue.clear();
        _data.value64 = 0;
        _data.state = PropertyValueData::Empty;
    }

    /**
     * @brief empty
     * @return  true if there is not stored data
     */
    inline bool empty() const
    {
        return _data.state == PropertyValueData::Empty;
    }

  protected:
    /**
     * @brief getPropertyDataFromVariant
     *
     *  Having a valid value stored in std::variant
     *    If it is any integer/float/double, this value is just casted into
     *       64 bits integer and sets valid=true.
     *    When this value is a string, calls string2Uint64()
     *     to check if it is a string representation of an integer,
     *     if so, converts the string into 64 bits integer and sets valid=true
     *     otherwise sets valid=false
     *
     * @param varVar
     */
    void getPropertyDataFromVariant(const PropertyVariant& varVar);

    /**
     * @brief  checks if 'value' is a representation of an integer
     *
     *         Sets all fiels from this->_data
     *
     * @param value  string to check
     *
     *
     */
    void string2Uint64(const std::string& value);

  protected:
    /**
     * @brief _data keeps both string and 64 bits integer representation of
     *              any data entered by PropertyVariant or just a string.
     */
    PropertyValueData _data;
};

/**
 * @brief The PropertyString does not perform any integer conversion
 */
class PropertyString : public PropertyValue
{
  public:
    explicit PropertyString(const std::string& value);
    explicit PropertyString(const PropertyVariant& varVar);
    PropertyString() = delete;
    ~PropertyString() = default;
};

namespace criteria
{
/**
 * @brief helper function to get redefined value if specified
 * @param redefCriteria
 * @param accessorValue
 * @return value from redefCriteria if that exists, otherwise accessorValue
 */
PropertyString getStringFromCriteria(const PropertyVariant& redefCriteria,
                                     const std::string& accessorValue);
/**
 * @brief helper function to get redefined value if specified
 * @param redefCriteria
 * @param accessorValue
 * @return value from redefCriteria if that exists, otherwise accessorValue
 */
PropertyValue getValueFromCriteria(const PropertyVariant& redefCriteria,
                                   const std::string& accessorValue);
} // namespace criteria

} // namespace data_accessor
