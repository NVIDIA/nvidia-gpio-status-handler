
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
                auto valRangRepeatPos =
                        myVal.find_first_of(util::RangeRepeaterIndicator);
                if (valRangRepeatPos != std::string::npos)
                {
                    myVal = util::revertRangeRepeated(myVal, valRangRepeatPos);
                }
                auto otherRangeRepeatPos =
                        otherVal.find_first_of(util::RangeRepeaterIndicator);
                if (otherRangeRepeatPos != std::string::npos)
                {
                    otherVal = util::revertRangeRepeated(otherVal,
                                                         otherRangeRepeatPos);
                }

                const std::regex r{myVal};
                auto values_match =  std::regex_match(otherVal, r);
                if (values_match == false)
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
               const std::string& deviceType = std::string{""}) const;

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
    std::string read(const std::string& device = std::string{""})
    {
        log_elapsed();
        std::string ret{"123"};
        log_dbg("device='%s'\n", device.c_str());

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
        else if (isTypeTest() == true)
        {
            return _acc[testValueKey];
        }
        else if (isTypeDeviceName())
        {
            return device;
        }
        else if (isTypeConst())
        {
            return _acc["value"].get<std::string>();
        }

        if (_dataValue.empty() == false)
        {
            ret = _dataValue.getString();
        }
        log_dbg("ret=%s\n", ret.c_str());
        return ret;
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
     * @brief  the @sa check() is supposed to fill _latestCheckedDevices
     *
     * @return the _latestAssertedDevices, a map with deviceId and deviceName
     */
    inline util::DeviceIdMap getAssertedDeviceNames() const
    {
        return _latestAssertedDevices;
    }

    /**
     * @brief just a part of the @sa check() logic, it intends for testing
     *        It should be when  otherAcc already has data (after @sa read())
     *
     * @param accData
     * @param other
     * @param eventType
     * @return
     */
    bool subCheck(const DataAccessor& otherAcc,
                  const PropertyVariant& redefCriteria,
                  const std::string& range, const std::string& dev2Read) const;

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
                               const std::string& deviceType) const;

    /**
     * @brief creates fills the _latestAssertedDevices from accData
     *               with a single deviceName
     *
     * @param accData     the DataAcessor to fill
     * @param realDevice  the device name
     * @param devType     the device type which can contain range specification
     */
    void buildSingleAssertedDeviceName(DataAccessor& accData,
                                       const std::string& realDevice,
                                       const std::string& devType) const;

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
     * @return populated map if type is CMDLINE and there is range in arguments
     *         otherwise an empty map
     */
    util::DeviceIdMap
        getCmdLineRangeArguments(const std::string& deviceType) const;

    /**
     * @brief performs a check() for all devices from 'devices' parameter
     *
     *        The Accessor::read(single-device) is performed on *this
     * @note
     *        *this and otherAcc CANNOT BE THE SAME OBJECT
     *
     * @param devices map of devicesId with their deviceNames
     * @param otherAcc the Accessor which will receive assertedDevices
     * @param redefCriteria another value from the one gotten from read() can
     *        be forced as the value to perform the check
     * @param deviceType the json device_type for current event
     * @return a single return, true if one or more checks return true
     */
    bool checkLoopingDevices(const util::DeviceIdMap& devices,
                             const DataAccessor& otherAcc,
                             const PropertyVariant& redefCriteria,
                             const std::string& deviceType);

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

    /**
     * @brief after calling check() it may contain the list of device names
     */
    util::DeviceIdMap _latestAssertedDevices;
};

} // namespace data_accessor
