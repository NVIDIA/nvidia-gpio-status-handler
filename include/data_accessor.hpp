
/*
 *
 */

#pragma once

#include "aml.hpp"
#include "dbus_accessor.hpp"
#include "property_accessor.hpp"
#include "util.hpp"

#include <nlohmann/json.hpp>

#include <iostream>
#include <map>
#include <regex>
#include <string>
#include <tuple>
#include <vector>

namespace event_info
{
class EventNode;
}

namespace data_accessor
{

/**
 *   Map Dbus Interface and a list of objects
 */
using InterfaceObjectsMap = std::map<std::string, std::vector<std::string>>;

constexpr auto typeKey = "type";
constexpr auto nameKey = "name";
constexpr auto checkKey = "check";
constexpr auto objectKey = "object";
constexpr auto interfaceKey = "interface";
constexpr auto propertyKey = "property";
constexpr auto executableKey = "executable";
constexpr auto argumentsKey = "arguments";
constexpr auto deviceNameKey = "device_name";
constexpr auto testValueKey = "test_value";
constexpr auto deviceidKey = "device_id";
constexpr auto executeBgKey = "execute_bg";
constexpr auto valueKey = "value";
constexpr auto readFailedReturn = "Value_Not_Available";

static std::map<std::string, std::vector<std::string>> accessorTypeKeys = {
    {"DBUS", {"object", "interface", "property"}},
    {"DeviceCoreAPI", {"property"}},
    {"DEVICE", {"device_name"}},
    {"OTHER", {"other"}},
    {"DIRECT", {}},
    {"CONSTANT", {"value"}}};

/**
 * @brief A class for Data Accessor
 *
 */
class DataAccessor
{
  public:
    DataAccessor() : _dataValue(PropertyValue())
    {}

    explicit DataAccessor(const nlohmann::json& acc,
                          const PropertyValue& value = PropertyValue())
       :  _acc(acc), _dataValue(value)
    {
        std::stringstream ss;
        ss << "Const.: _acc: " << _acc;
        log_dbg("%s\n", ss.str().c_str());
    }

    /**
     *  @brief  used for tests purpose with an invalid accessor type
     */
    explicit DataAccessor(const PropertyVariant& initialData) :
        _dataValue(PropertyValue())
    {
        setDataValueFromVariant(initialData);
    }

    ~DataAccessor() = default;

  public:
    /**
     * @brief Print this object to the output stream @c os (e.g. std::cout,
     * std::cerr, std::stringstream) with every line prefixed with @c indent.
     *
     * For the use with logging framework use the following construct:
     *
     * @code
     *   std::stringstream ss;
     *   obj.print(ss, indent);
     *   log_dbg("%s", ss.str().c_str());
     * @endcode
     */
    template <class CharT>
    void print(std::basic_ostream<CharT>& os = std::cout,
               std::string indent = std::string("")) const
    {
        os << indent << this->_acc << std::endl;
    }

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
            std::stringstream ss;
            ss << "not valid: acc = " << acc;
            log_dbg("%s\n", ss.str().c_str());
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

                auto otherVal = other._acc[key].get<std::string>();
                auto myVal = val.get<std::string>();
                if (util::matchRegexString(myVal, otherVal) == false)
                {
                    ret = false;
                    break;
                }
            }
        }
        std::stringstream ss;
        ss << "\n\tThis: " << _acc << "\n\tOther: " << other._acc
           << "\n\treturn: " << ret;
        log_dbg("%s\n", ss.str().c_str());
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
     * @brief checks if _acc["check"]["bitmap"] exists
     * @return true if that exists, otherwise false
     */
    bool existsCheckBitmap() const
    {
        return existsCheckKey() && _acc[checkKey].count(bitmapKey) != 0;
    }

    /**
     * @brief checks if _acc["check"]["lookup"] exists
     * @return true if that exists, otherwise false
     */
    inline bool existsCheckLookup() const
    {
        return existsCheckKey() && _acc[checkKey].count(lookupKey) != 0;
    }

    /**
     * @brief This is an optional flag telling the Accessor to loop devices
     *
     * The “device_id” field if present can have the following values:
     *   1. “device_id”: “range”
     *   2. “device_id”: “single”
     *
     * If the “device_id” field  is NOT present if defaults to "range"
     *
     * @return true if “device_id” is not present or if it is equal "range"
     */
    inline bool isDeviceIdRange() const
    {
        return _acc.count(deviceidKey) == 0 || _acc[deviceidKey] == "range";
    }

    /**
     * @brief read value per the accessor info
     *
     * @return std::string
     */
    std::string read(const std::string& device = std::string{""},
                     const std::string& deviceType = std::string{""})
    {
        log_elapsed();
        log_dbg("device='%s'\n", device.c_str());

        if (isTypeDbus() == true)
        {
            readDbus(device);
        }
        else if (isTypeDevice() == true)
        {
            _dataValue = PropertyValue(_acc[deviceNameKey].get<std::string>());
        }
        else if (isTypeCmdline() == true)
        {
            runCommandLine(device, deviceType);
        }
        else if (isTypeDeviceCoreApi() == true)
        {
            readDeviceCoreApi(device);
        }
        else if (isTypeTest() == true)
        {
            _dataValue = PropertyValue(_acc[testValueKey].get<std::string>());
        }
        else if (isTypeDeviceName())
        {
            _dataValue = PropertyValue(device);
        }
        else if (isTypeConst())
        {
            _dataValue = PropertyValue(_acc[valueKey].get<std::string>());
        }

        if (_dataValue.empty() == false)
        {
            auto ret = _dataValue.getString();
            log_dbg("ret='%s'\n", ret.c_str());
            return ret;
        }

        log_dbg("read failed, returning data_accessor::readFailedReturn='%s'\n", data_accessor::readFailedReturn);
        return std::string{data_accessor::readFailedReturn};
    }

    std::string read(const event_info::EventNode& event);

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
     * @brief helper function to get the Dbus Object Path
     * @return
     */
    inline std::string getDbusObjectPath() const
    {
        std::string ret{""};
        if (isValidDbusAccessor() == true)
        {
            ret = _acc[objectKey].get<std::string>();
        }
        return ret;
    }

    /**
     * @brief helper function to get the Property
     * @return
     */
    inline std::string getProperty() const
    {
        std::string ret{""};
        if (isValid(_acc) == true && _acc.count(propertyKey) != 0)
        {
            ret = _acc[propertyKey].get<std::string>();
        }
        return ret;
    }

    /**
     * @brief helper function to get the Dbus Interface
     * @return
     */
    inline std::string getDbusInterface() const
    {
        std::string ret{""};
        if (isValidDbusAccessor() == true)
        {
            ret = _acc[interfaceKey].get<std::string>();
        }
        return ret;
    }

    /**
     * @brief returns a map of interface and a list objects-path expanded
     *
     *        A object path like : /xyz/blabla/GPU[0-7] generates a list of 8
     *          objects.
     * @return
     */
    InterfaceObjectsMap getDbusInterfaceObjectsMap() const;

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
     * @brief isTypeTest()
     *
     * @return  true if acccessor["type"] exists and it is TEST
     */
    inline bool isTypeTest() const
    {
        return isValid(_acc) == true && _acc[typeKey] == "TEST";
    }

    /**
     * @brief isTypeDeviceName()
     *
     * @return  true if acccessor["type"] exists and it is "DIRECT"
     */
    inline bool isTypeDeviceName() const
    {
        return isValid(_acc) == true && _acc[typeKey] == "DIRECT";
    }

    /**
     * @brief isTypeConst()
     *
     * @return  true if acccessor["type"] exists and it is "DIRECT"
     */
    inline bool isTypeConst() const
    {
        return isValid(_acc) == true && _acc[typeKey] == "CONSTANT";
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
     * @brief hasData() checks if a real data is stored in _dataValue
     *
     *                  It should return true after read() gets a real data
     *
     * @return true if there is some data stored
     */
    inline bool hasData() const
    {
        return _dataValue.empty() == false;
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
       return _dataValue;
    }

    /**
     * @brief   checks if it is Dbus type and if mandatory fields are present
     *
     * @return true if this Accessor Dbus is OK
     */
    inline bool isValidDbusAccessor() const
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
    inline bool isValidCmdlineAccessor() const
    {
        return isTypeCmdline() == true && _acc.count(executableKey) != 0;
    }

    /**
     * @brief isValidDeviceCoreApiAccessor()
     *
     * @return true if the Accessor type "DeviceCoreAPI" has property
     */
    inline bool isValidDeviceCoreApiAccessor() const
    {
        return isTypeDeviceCoreApi() == true && _acc.count(propertyKey) != 0;
    }

    /**
     * @brief isValidDeviceNameAccessor()
     *
     * @return true if acccessor["type"] exists and it is "DIRECT"
     */
    bool isValidDeviceNameAccessor() const
    {
        return isTypeDeviceName();
    }

    /**
     * @brief isValidConstantAccessor()
     *
     * @return true if acccessor["type"] exists and it is "CONSTANT"
     */
    bool isValidConstantAccessor() const
    {
        return isTypeDeviceName();
    }

    /**
     * @brief Intends to  behave like DataAccessor::read(device) but using
     *        information from another DataAccessor
     *
     *     Cases when it works:
     *     1.  otherAcc is equal *this and otherAcc already has data
     *            [ Copy otherAcc data ]
     *     2.  both *this and  otherAcc are DBUS
     *         2.1 *this has range in object path, otherAcces does not have
     *            2.1.1 But both objects match not considering range
     *              [ *this perform read() but using otherAcc object path
     *
     * @param otherAcc that may match with this
     *
     * @return empty string if could not get any valid data from other Accessor
     *     or a good data based on otherAcc information
     */
    std::string readUsingMainAccessor(const DataAccessor& otherAcc);

    /**
         * @brief  returns a DeviceIdMap from arguments in accessor type CMDLINE
         *
         *    For an Accessor such as:
         * @code
         *        "accessor": {
         *          "type": "CMDaLINE",
         *          "executable": "mctp-vdm-util-wrapper",
         *          "arguments": "bla GPU[0-7]",
         *          ...
         *         }
         * @endcode
         *      returns a map => 0=GPU0 1=GPU1, ...
         *
         * @return populated map if type is CMDLINE and has range in arguments
         *         otherwise an empty map
         */
    util::DeviceIdMap
    getCmdLineRangeArguments(const std::string& deviceType) const;

 private:
    /**
     * @brief clearData() clear the _dataValue if it has a previous value
     */
    inline void clearData()
    {
        _dataValue.clear();
    }

    /**
     * @brief readDbus() reads the property contained in _acc
     *
     *     _acc["object], _acc["interface"] and _acc["property"]
     *
     * @return true if the read operation was successful, false otherwise
     */
    bool readDbus(const std::string& device);

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
    bool runCommandLine(const std::string& device = std::string{""},
                        const std::string& devType = std::string{""});

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
    PropertyValue _dataValue;
};

} // namespace data_accessor
