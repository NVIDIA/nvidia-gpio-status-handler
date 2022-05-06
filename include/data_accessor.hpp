
/*
 *
 */

#pragma once

#include "dbus_accessor.hpp"
#include "property_accessor.hpp"
#include "util.hpp"

#include <nlohmann/json.hpp>

#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <tuple>
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
    {"DeviceCoreAPI", {"property"}},
    {"DEVICE", {"device_name"}},
    {"OTHER", {"other"}},
};

/**
 * @brief A class for Data Accessor
 *
 */
class DataAccessor
{
  public:
    DataAccessor() : _dataValue(nullptr)
    {}

    explicit DataAccessor(const nlohmann::json& acc) :
        _acc(acc), _dataValue(nullptr)
    {
#ifdef ENABLE_LOGS
        std::cout << "Const.: _acc: " << _acc << "\n";
#endif
    }

    /**
     *  @brief  used for tests purpose with an invalid accessor type
     */
    explicit DataAccessor(const PropertyVariant& initialData) :
        _dataValue(nullptr)
    {
        setDataValueFromVariant(initialData);
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
#ifdef ENABLE_LOGS
            std::cout << "not valid: acc = " << acc << "\n";
#endif
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
        bool ret = isValid(other._acc) && _acc[typeKey] == other._acc[typeKey];
        if (ret == true)
        {
            for (auto& [key, val] : _acc.items())
            {
                if (key == typeKey || key == checkKey) // skip check key
                {
                    continue;
                }
                if (other._acc.count(key) == 0)
                {
                    ret = false;
                    break;
                }

                const std::regex r{val}; // 'val' could have regex format
                auto values_match =
                    std::regex_match(other._acc[key].get<std::string>(), r);
                if (values_match == false)
                {
                    ret = false;
                    break;
                }
            }
        }
#ifdef ENABLE_LOGS
        std::cout << __PRETTY_FUNCTION__ << "\n\tThis: " << _acc
                  << "\n\tOther: " << other._acc << "\n\treturn: " << ret
                  << std::endl;
#endif
        return ret;
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
     * @brief performs a check operation of its criteria against otherAcc data
     *
     *   This DataAccessor has the DataAccessor operation criteria
     *
     * @param otherAcc contains the data the checked again this criteria*
     *        otherAcc calls read() get its data.
     *
     * @param [optional] redefCriteria  other value to be used in this
     *                   DataAccessor criteria.
     *
     * @param [optional] device  the device name passed into read()
     *
     * redefinition of 'bitmask' criteria'
     * @code:
     *   const nlohmann::json json = {
     *     {"type", "DBUS"},
     *     {"object", "/xyz/openbmc_project/inventory/system/chassis/GPU0"},
     *     {"interface", "xyz.openbmc_project.Inventory.Decorator.Dimension"},
     *     {"property", "Depth"},
     *     {"check", {{"bitmask", "0x01"}}}
     *   };
     *
     * // supposing the property above of GPU0 having the value 271 = 0x10
     *   DataAccessor(json).check() // true using 0x01 from current criteria
     *
     * // The following Criteria call examples with redefinition return true:
     *   DataAccessor(json).check(PropertyVariant(uint64_t(0x02)); // bit 1 set
     *   DataAccessor(json).check(PropertyVariant(uint64_t(0x04)); // bit 2 set
     *   DataAccessor(json).check(PropertyVariant(uint64_t(0x08)); // bit 3 set
     *
     * // The following call returns false
     *   DataAccessor(json).check(PropertyVariant(uint64_t(0x10)); // bit 4 NOT
     * @endcode
     *
     * redefinition of 'lookup' criteria
     * @code
     *  const nlohmann::json json = {
     *      {"type", "CMDLINE"},
     *      {"executable", "/bin/echo"},
     *      {"arguments", "ff 00 00 00 00 00 02 40 66 28"},
     *      {"check", {{"lookup", "00 02 40"}}}
     *  };
     *
     *  DataAccessor(json).check(PropertyVariant(std::string{"40 6"})); // true
     *  DataAccessor(json).check(PropertyVariant(std::string{"zz"}));   // false
     * @endcode
     *
     * @return true of the criteria matches the value
     */
    bool check(const DataAccessor& otherAcc,
               const PropertyVariant& redefCriteria = PropertyVariant(),
               const std::string& device = std::string{""}) const;

    /**
     * @brief [overloaded] with inverted order of device and redefCriteria
     */
    bool check(const DataAccessor& otherAcc, const std::string& device,
               const PropertyVariant& redefCriteria = PropertyVariant()) const;
    /**
     * @brief [overloaded] using otherAcc = *this and device
     */
    bool check(const std::string& device,
               const PropertyVariant& redefCriteria = PropertyVariant()) const;
    /**
     * @brief [overloaded] using otherAcc = *this and redefCriteria
     */
    bool check(const PropertyVariant& redefCriteria,
               const std::string& device = std::string{""}) const;

    /**
     * @brief [overloaded] using otherAcc = *this,
     *                           redefCriteria = PropertyVariant()
     *                           empty device
     */
    bool check() const;

    /**
     * @brief performs a @sa check() against thisData and against other
     *
     *        This is usually a 'event trigger' Accessor
     *
     * @param accData  Accessor which was created when Propety has changed
     *
     * @param other     Accessor which will check iself in device loop
     *
     * @param eventType The device type which is used to expand device names
     *
     * @return a list of device names that check matches
     */
    bool check(const DataAccessor& accData, const DataAccessor& other,
               const std::string& eventType);

    /**
     * @brief Access accessor just like do it on json
     *
     * @param key
     * @return auto
     */
    auto operator[](const std::string& key) const
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
    auto count(const std::string& key) const
    {
        return _acc.count(key);
    }

    /**
     * @brief determine if _acc is empty json object or not
     *
     * @return bool
     */
    bool isEmpty(void) const
    {
        return _acc.empty();
    }

    /**
     * @brief checks if accessor["check"] exists
     *
     * @return true if that exists
     */
    inline bool existsCheckKey() const
    {
        return isEmpty() == false && _acc.count(checkKey) != 0;
    }

    /**
     * @brief checks if _acc["check"]["bitmask"] exists
     * @return true if that exists, otherwise false
     */
    bool existsCheckBitmask() const
    {
        return existsCheckKey() && _acc[checkKey].count(bitmaskKey) != 0;
    }

    /**
     * @brief read value per the accessor info
     *
     * @return std::string
     */
    std::string read(const std::string& device = std::string{""})
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
            runCommandLine(device);
        }
        else if (isTypeDeviceCoreApi() == true)
        {
            readDeviceCoreApi(device);
        }

        if (_dataValue != nullptr)
        {
            ret = _dataValue->getString();
        }
#ifdef ENABLE_LOGS
        std::cout << __PRETTY_FUNCTION__ << "(): "
                  << "ret=" << ret << std::endl;
#endif
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

    /**
     * @brief  the @sa check() is supposed to fill _latestCheckedDevices
     *
     * @return the _latestAssertedDevices, a map with deviceId and deviceName
     */
    inline util::DeviceIdMap getAssertedDeviceNames() const
    {
        return _latestAssertedDevices;
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
     * @brief isTypeDeviceCoreApi()
     *
     * @return true if acccessor["type"] exists and it is "DeviceCoreAPI"
     */
    inline bool isTypeDeviceCoreApi() const
    {
        return isValid(_acc) == true && _acc[typeKey] == "DeviceCoreAPI";
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
     * @brief getDataValue() instead of read() it returns the real data
     *
     * @note  It does not call read(), just returns the data if exists
     *
     * @return returns the data if exists, otherwise an invalid PropertyValue
     */
    inline PropertyValue getDataValue() const
    {
        PropertyValue data;
        if (hasData() == true)
        {
            data = *_dataValue;
        }
        return data;
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
     * @brief isValidDeviceCoreApiAccessor()
     *
     * @return true if the Accessor type "DeviceCoreAPI" has property
     */
    bool isValidDeviceCoreApiAccessor() const
    {
        return isTypeDeviceCoreApi() && _acc.count(propertyKey) != 0;
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
    bool runCommandLine(const std::string& device = std::string{""});

    /**
     * @brief   just initializes the _dataValue creating a PropertyVariant
     *
     * @param propVariant the value itself
     *
     * @return true if the propVariant has a valid data, otherwise false
     */
    bool setDataValueFromVariant(const PropertyVariant& propVariant);

    /**
     * @brief readDeviceCoreApi() reads data for Accessor "DeviceCoreAPI"
     *
     * @return true if the propVariant has a valid data, otherwise false
     */
    bool readDeviceCoreApi(const std::string& device);

    /**
     * @brief looks in other["object"] and this["object"] and in device
     *          to find a device name
     * @param other
     * @param device
     * @return a valid device name such as "GPU1" or empty string
     */
    std::string findDeviceName(const DataAccessor& other,
                               const std::string& device) const;

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

    /**
     * @brief after calling check() it may contain the list of device names
     */
    util::DeviceIdMap _latestAssertedDevices;
};

} // namespace data_accessor
