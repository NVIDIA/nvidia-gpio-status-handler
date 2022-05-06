/*
 *
 */
#include "data_accessor.hpp"

#include "util.hpp"

#include <boost/process.hpp>

#include <regex>
#include <string>
#include <vector>

namespace data_accessor
{

bool DataAccessor::contains(const DataAccessor& other) const
{
    bool ret = isValid(other._acc);
    if (ret == true)
    {
        for (auto& [key, val] : other._acc.items())
        {
            if (_acc.count(key) == 0)
            {
#ifdef ENABLE_LOGS
                std::cout << __PRETTY_FUNCTION__ << "(): "
                          << "key=" << key << " not found in _acc" << std::endl;
#endif
                ret = false;
                break;
            }
            // this may have
            auto regex_string = _acc[key].get<std::string>();
            auto val_string = val.get<std::string>();
            const std::regex reg_value{regex_string};
            auto values_match = std::regex_match(val_string, reg_value);
            if (values_match == false)
            {
#ifdef ENABLE_LOGS
                std::cout << __PRETTY_FUNCTION__ << "(): "
                          << " values do not match key=" << key
                          << " value=" << val << std::endl;
#endif
                ret = false;
                break;
            }
        }
    }
#ifdef ENABLE_LOGS
    std::cout << __PRETTY_FUNCTION__ << "(): "
              << "ret=" << ret << std::endl;
#endif
    return ret;
}

bool DataAccessor::check(const DataAccessor& otherAcc,
                         const PropertyVariant& redefCriteria,
                         const std::string& device) const
{
    bool ret = true; // defaults to true if accessor["check"] does not exist
    if (existsCheckKey() == true)
    {
        bool deviceHasRange = util::existsRange(device);
        std::string deviceToRead = findDeviceName(otherAcc, device);
        DataAccessor& accNonConst = const_cast<DataAccessor&>(otherAcc);
        accNonConst.read(deviceToRead);
        if ((ret = accNonConst.hasData()) == true)
        {
            auto checkMap = _acc[checkKey].get<CheckDefinitionMap>();
            /**
             * special logic for bitmask
             * --------------------------
             *   bitmask should match for deviceId present in either:
             *    1. deviceToRead and there is no regex in device
             *    2. device is regex, i.e, comes from 'event.device_type'
             */
            bool hasDevice = deviceHasRange || deviceToRead.empty() == false;
            bool bitM = existsCheckBitmask();
            bool bitmaskNoRedefined = redefCriteria.index() == 0;
            if (hasDevice == true && bitM == true && bitmaskNoRedefined == true)
            {
                ret = false; // set to false
                util::DeviceIdMap devices =
                    (deviceHasRange == true)
                        ? util::expandDeviceRange(device)
                        : util::expandDeviceRange(deviceToRead);
                // now walk thuru devices
                for (auto& deviceItem : devices)
                {
                    auto devId = deviceItem.first;
                    PropertyVariant bitmask(uint64_t(1 << devId));
                    if (otherAcc._dataValue->check(checkMap, bitmask) == true)
                    {
                        const auto& deviceName = deviceItem.second;
                        ret = true; // true if at least one device
                        accNonConst._latestAssertedDevices[devId] = deviceName;
                    }
                }
            }
            else // common case
            {
                ret = otherAcc._dataValue->check(checkMap, redefCriteria);
            }
        }
    } // existsCheckKey() == true
    return ret;
}

bool DataAccessor::check(const DataAccessor& otherAcc,
                         const std::string& device,
                         const PropertyVariant& redefCriteria) const
{
    return check(otherAcc, redefCriteria, device);
}

bool DataAccessor::check(const PropertyVariant& redefCriteria,
                         const std::string& device) const
{
    return check(*this, redefCriteria, device);
}

bool DataAccessor::check(const std::string& device,
                         const PropertyVariant& redefCriteria) const
{
    return check(*this, redefCriteria, device);
}

bool DataAccessor::check() const
{
    return check(*this, PropertyVariant(), std::string{""});
}

bool DataAccessor::check(const DataAccessor& accData, const DataAccessor& other,
                         const std::string& eventType)
{
    bool ret = check(accData, eventType); // first event_trigger check
    if (ret == true && other.existsCheckKey() == true)
    {
        ret = other.check(eventType);
        if (ret == true)
        {
            auto latestCheckedDevices = other._latestAssertedDevices;
            if (latestCheckedDevices.empty() == false)
            {
                DataAccessor& accNonConst = const_cast<DataAccessor&>(accData);
                DataAccessor& otherNonConst = const_cast<DataAccessor&>(other);
                otherNonConst._latestAssertedDevices.clear();
                accNonConst._latestAssertedDevices = latestCheckedDevices;
            }
        }
    }
    return ret;
}

bool DataAccessor::readDbus()
{
    clearData();
    bool ret = isValidDbusAccessor();
    if (ret == true)
    {
        auto propVariant = dbus::readDbusProperty(
            _acc[objectKey], _acc[interfaceKey], _acc[propertyKey]);
        ret = setDataValueFromVariant(propVariant);
    }
#ifdef ENABLE_LOGS
    std::cout << __PRETTY_FUNCTION__ << "(): "
              << "ret=" << ret << std::endl;
#endif
    return ret;
}

bool DataAccessor::runCommandLine(const std::string& device)
{
    clearData();
    bool ret = isValidCmdlineAccessor();
    if (ret == true)
    {
        auto cmd = _acc[executableKey].get<std::string>();
        if (_acc.count(argumentsKey) != 0)
        {
            auto args = _acc[argumentsKey].get<std::string>();
            if (device.empty() == false)
            {
                args = util::replaceRangeByMatchedValue(args, device);
            }
            else // expand the range inside args
            {
                // TODO: "before A[0-3] after" => "before A0 A1 A2 A3 after"
                // args = expandRangeFromString(args);
            }
            cmd += ' ' + args;
        }
#ifdef ENABLE_LOGS
        std::cout << "[D] " << __PRETTY_FUNCTION__ << "() "
                  << "cmd: " << cmd << std::endl;
#endif
        std::string result{""};
        try
        {
            std::string line{""};
            boost::process::ipstream pipe_stream;
            boost::process::child process(cmd, boost::process::std_out >
                                                   pipe_stream);
            while (pipe_stream && std::getline(pipe_stream, line) &&
                   line.empty() == false)
            {
                result += line;
            }
            process.wait();
        }
        catch (const std::exception& error)
        {
            ret = false;
            std::cerr << "[E]"
                      << "Error running the command \'" << cmd << "\' "
                      << error.what() << std::endl;
        }
        // ok lets store the output
        if (ret == true)
        {
            _dataValue =
                std::make_shared<PropertyValue>(PropertyString(result));
        }
    }
    return ret;
}

bool DataAccessor::setDataValueFromVariant(const PropertyVariant& propVariant)
{
    clearData();
    bool ret = propVariant.index() != 0;
    // index not zero, not std::monostate that means valid data
    if (ret == true)
    {
        _dataValue =
            std::make_shared<PropertyValue>(PropertyValue(propVariant));
    }
    return ret;
}

bool DataAccessor::readDeviceCoreApi(const std::string& device)
{
    clearData();
    bool ret = isValidDeviceCoreApiAccessor();
    if (ret == true)
    {
        auto deviceId = util::getDeviceId(device);
        auto tuple = dbus::deviceGetCoreAPI(deviceId, _acc[propertyKey]);
        auto rc = std::get<int>(tuple);
        if (rc != 0)
        {
            ret = false;
        }
        else
        {
            _dataValue = std::make_shared<PropertyValue>(PropertyValue(
                std::get<std::string>(tuple), std::get<uint64_t>(tuple)));
        }
    }
    return ret;
}

std::string DataAccessor::findDeviceName(const DataAccessor& other,
                                         const std::string& device) const
{
    std::string deviceName = util::getDeviceName(device);
    if (deviceName.empty() == true && other.count(objectKey) != 0)
    {
        deviceName = util::getDeviceName(other[objectKey].get<std::string>());
    }
    if (deviceName.empty() == true && _acc.count(objectKey) != 0)
    {
        deviceName = util::getDeviceName(_acc[objectKey].get<std::string>());
    }
    return deviceName;
}

} // namespace data_accessor
