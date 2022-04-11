
/*
 *
 */

#pragma once

#include "property_accessor.hpp"

#include <nlohmann/json.hpp>

#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <vector>

namespace data_accessor
{

constexpr auto typeKey = "type";
constexpr auto nameKey = "name";
constexpr auto checkKey = "check";
constexpr auto objectKey = "object";
constexpr auto interfaceKey = "interface";
constexpr auto propertyKey = "property";
constexpr auto executableKey = "executable";
constexpr auto argumentsKey = "arguments";
constexpr auto deviceNameKey = "device_name";

static std::map<std::string, std::vector<std::string>> accessorTypeKeys = {
    {"DBUS", {"object", "interface", "property"}},
    {"DEVICE", {"device_name"}},
    {"OTHER", {"other"}}};

/**
 * @brief A class for Data Accessor
 *
 */
class DataAccessor
{
  public:
    DataAccessor() : _dataValue(nullptr)
    {}

    DataAccessor(const nlohmann::json& acc) : _acc(acc), _dataValue(nullptr)
    {
        std::cout << "Const.: _acc: " << _acc << "\n";
    }

    ~DataAccessor() = default;

  public:
    /**
     * @brief Assign json data.
     *
     * @param acc
     * @return nlohmann::json&
     */
    nlohmann::json& operator=(const nlohmann::json& acc)
    {
        if (!isValid(acc))
        {
            std::cout << "not valid: acc = " << acc << "\n";
            return _acc;
        }

        _acc = acc;
        return _acc;
    }

    /**
     * @brief Comparison operator of the accessor
     *
     * @param other
     * @return true
     * @return false
     */
    bool operator==(const DataAccessor& other)
    {
        std::cout << "This: " << _acc << ", Other: " << other._acc << "\n";
        if (!isValid(other._acc))
        {
            return false;
        }
        if (_acc[typeKey] != other._acc[typeKey])
        {
            return false;
        }
        for (auto& [key, val] : _acc.items())
        {
            std::cout << "key: " << key << ", val: " << val << "\n";
            if (key == typeKey || key == checkKey) // skip check key
            {
                continue;
            }
            else
            {
                if (other._acc.count(key) == 0)
                {
                    // the key is not in 'other'
                    std::cout << "Other has no key. False.\n";
                    return false;
                }

                const std::regex r{val}; // 'val' could have regex format
                return std::regex_match(other._acc[key].get<std::string>(), r);
            }
        }
        return _acc == other._acc;
    }

    /**
     * @brief contains() checks if other is a sub set of this
     * @param other
     * @return true all fields from other match with this
     *         false in case fields from other are not present in this or
     *               their content do not match
     */
    bool contains(const DataAccessor& other) const;

    /**
     * @brief check() checks the if the value in '_dataValue'
     *                matches with other DataAccessor criteria
     *
     *                The other DataAccessor criteria is like:
     *
     *                   {"check": {
     *                               { "bitmask" , "0x01" }
     *                             }
     *                   }
     *                Or:
     *                   {"check": {
     *                               { "lookup" , "1234" }
     *                             }
     *                   }
     *
     *    @sa read() or @sa readDbusProperty()
     *
     * @return true if the property value matches the criteria in other
     */
    bool check(const DataAccessor& acc) const;

    /**
     * @brief performs the check against itself
     *
     * @return
     */
    bool check() const;

    /**
     * @brief Access accessor just like do it on json
     *
     * @param key
     * @return auto
     */
    auto operator[](const std::string& key)
    {
        return _acc[key];
    }

    /**
     * @brief Output the accessor details
     *
     * @param os
     * @param acc
     * @return std::ostream&
     */
    friend std::ostream& operator<<(std::ostream& os, const DataAccessor& acc)
    {
        os << acc._acc;
        return os;
    }

    /**
     * @brief Return key count of json
     *
     * @param key
     * @return auto
     */
    auto count(const std::string& key)
    {
        return _acc.count(key);
    }

    /**
     * @brief determine if _acc is empty json object or not
     *
     * @return bool
     */
    bool isEmpty(void)
    {
        return _acc.empty();
    }

    /**
     * @brief read value per the accessor info
     *
     * @return std::string
     */
    std::string read(void)
    {
        std::string ret{"123"};

        if (isTypeDbus() == true)
        {
            readDbus();
        }
        else if (isTypeDevice() == true)
        {
            return _acc[deviceNameKey];
        }
        else if (isTypeCmdline() == true)
        {
            runCommandLine();
        }
        if (_dataValue != nullptr)
        {
            ret = _dataValue->getString();
        }
        std::cout << __PRETTY_FUNCTION__ << "(): "
                  << "ret=" << ret << std::endl;
        return ret;
    }

    /**
     * @brief write value via the accessor info
     *
     * @param val
     */
    void write([[maybe_unused]] const std::string& val)
    {
        return;
    }

    /**
     * @brief   checks if it is Device type and if mandatory fields are present
     *
     * @return true if this Accessor Device is OK
     */
    bool isValidDeviceAccessor() const
    {
        return isTypeDevice() == true && _acc.count(deviceNameKey) != 0;
    }

  private:
    /**
     * @brief Check if a acc json has the "type" field.
     *
     * @param acc
     * @return true
     * @return false
     */
    bool isValid(const nlohmann::json& acc) const
    {
        return (acc.count(typeKey) > 0);
    }

    /**
     * @brief isTypeDbus()
     *
     * @return  true if acccessor["type"] exists and it is DBUS
     */
    inline bool isTypeDbus() const
    {
        return isValid(_acc) == true && _acc[typeKey] == "DBUS";
    }

    /**
     * @brief isTypeDevice()
     *
     * @return  true if acccessor["type"] exists and it is DEVICE
     */
    inline bool isTypeDevice() const
    {
        return isValid(_acc) == true && _acc[typeKey] == "DEVICE";
    }

    /**
     * @brief isTypeCmdline()
     *
     * @return  true if acccessor["type"] exists and it is CMDLINE
     */
    inline bool isTypeCmdline() const
    {
        return isValid(_acc) == true && _acc[typeKey] == "CMDLINE";
    }

    /**
     * @brief hasData() checks if a real data is stored in _dataValue
     *
     *                  It should return true after read() gets a real data
     *
     * @return true if there is some data stored
     */
    inline bool hasData() const
    {
        return _dataValue != nullptr;
    }

    /**
     * @brief clearData() clear the _dataValue if it has a previous value
     */
    void clearData()
    {
        if (_dataValue != nullptr)
        {
            _dataValue.reset();
            _dataValue = nullptr;
        }
    }

    /**
     * @brief   checks if it is Dbus type and if mandatory fields are present
     *
     * @return true if this Accessor Dbus is OK
     */
    bool isValidDbusAccessor() const
    {
        return isTypeDbus() == true && _acc.count(objectKey) != 0 &&
               _acc.count(interfaceKey) != 0 && _acc.count(propertyKey) != 0;
    }

    /**
     * @brief   checks if it is CMDLINE type and if executable field is present
     *
     *          The arguments field is optional
     *
     * @return true if this Accessor CMDLINE is OK
     */
    bool isValidCmdlineAccessor() const
    {
        return isTypeCmdline() == true && _acc.count(executableKey) != 0;
    }

    /**
     * @brief readDbus() reads the property contained in _acc
     *
     *     _acc["object], _acc["interface"] and _acc["property"]
     *
     * @return true if the read operation was successful, false otherwise
     */
    bool readDbus();

    /**
     * Runs commands from Accessor type CMDLINE
     * Accessor example:
     * accessor": {
     *       "type": "CMDLINE",
     *       "executable": "mctp-vdm-util",
     *       "arguments": "-c query_boot_status -t 32",
     *       "check": {
     *             "lookup": "00 02 40"
     *       }
     */
    bool runCommandLine();

  private:
    /**
     * @brief hold json data for the accessor.
     *
     */
    nlohmann::json _acc;

    /**
     * @brief _dataValue stores the data value
     *
     * @sa read()
     */
    std::shared_ptr<PropertyValue> _dataValue;
};

} // namespace data_accessor
