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
                         const std::string& deviceType) const
{
    bool ret = true; // defaults to true if accessor["check"] does not exist
    std::string deviceToRead = findDeviceName(otherAcc, deviceType);
    DataAccessor& accNonConst = const_cast<DataAccessor&>(otherAcc);
    if (existsCheckKey() == true)
    {
        ret = false;
        accNonConst._latestAssertedDevices.clear();
        /*
         * check accessor type CMDLINE with range in arguments
         * if so, is necessary to expand devices range and check() each device
         */
        auto cmdLineRange = getCmdLineRangeInformationInArguments();
        auto sizeRangeString = std::get<0>(cmdLineRange);
        if (sizeRangeString > 0)
        {
            // final assertedDeviceNames output from the loop
            util::DeviceIdMap cmdlineAssertedDevices;
            auto postionRangeSring = std::get<1>(cmdLineRange);
            auto rangeString = std::get<2>(cmdLineRange);
            // devRange is expanded to 0=GPU0, 1=GPU1, ...
            auto devRange = util::expandDeviceRange(rangeString);
#ifdef ENABLE_LOGS
            std::cout << __FILE__ << ":" << __LINE__
                      << " assertedDeviceNames CMDLINE: " << rangeString
                      << std::endl;
#endif
            for (auto& arg : devRange)
            {
                DataAccessor cmdlineAcc = *this;
                std::string arguments = cmdlineAcc._acc[argumentsKey];
                arguments.replace(postionRangeSring, sizeRangeString,
                                  arg.second);
                cmdlineAcc._acc[argumentsKey] = arguments;
                cmdlineAcc.read();
                if (cmdlineAcc.subCheck(cmdlineAcc, redefCriteria, rangeString,
                                        arg.second) == true)
                {
                    ret = true;
                    // insert the asserteDeviceName from cmdlineAcc into
                    //  return map, it should always come as devId 0
                    if (cmdlineAcc._latestAssertedDevices.count(0) != 0)
                    {
                        auto& devId = arg.first;
                        cmdlineAssertedDevices[devId] =
                            cmdlineAcc._latestAssertedDevices[0];
                    }
                }
            }
            if (ret == true)
            {
                // the final assertedDeviceNames for CMDLINE with range in args
                accNonConst._latestAssertedDevices = cmdlineAssertedDevices;
                return ret; // work done
            }
        }
        else // not CMDLINE with range in arguments
        {
            accNonConst.read(deviceToRead);
            ret = subCheck(otherAcc, redefCriteria, deviceType, deviceToRead);
        }
    } // existsCheckKey() == true
    if (ret == true && accNonConst._latestAssertedDevices.empty() == true)
    {
        buildSingleAssertedDeviceName(accNonConst, deviceToRead, deviceType);
    }
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

bool DataAccessor::subCheck(const DataAccessor& otherAcc,
                            const PropertyVariant& redefCriteria,
                            const std::string& deviceType,
                            const std::string& dev2Read = std::string{""}) const
{
    if (otherAcc.hasData() == false)
    {
        return false; // without data nothing to do
    }
    bool ret = false;
    DataAccessor& accNonConst = const_cast<DataAccessor&>(otherAcc);
    auto checkMap = _acc[checkKey].get<CheckDefinitionMap>();
    /**
     * special logic for bitmask
     * --------------------------
     *   bitmask should match for deviceId present in either:
     *    1. deviceToRead and there is no regex in device
     *    2. device is regex, i.e, comes from 'event.device_type'
     */
    PropertyValue bitmapValue; // index is zero
    if (existsCheckBitmap() == true)
    {
        bitmapValue =
            isValidVariant(redefCriteria) == true
                ? PropertyValue(redefCriteria)
                : PropertyValue(_acc[checkKey][bitmapKey].get<std::string>());
    }
    if (bitmapValue.isValid() == true)
    {
        util::DeviceIdMap devices = util::expandDeviceRange(deviceType);
        // now walk thuru devices
        for (auto& deviceItem : devices)
        {
            auto devId = deviceItem.first;
            PropertyVariant bitmask(bitmapValue.getInt64() << devId);
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
        if (ret == true)
        {
            // it allows using subCheck() in UT
            buildSingleAssertedDeviceName(accNonConst, dev2Read, deviceType);
        }
    }
    return ret;
}

InterfaceObjectsMap DataAccessor::getDbusInterfaceObjectsMap() const
{
    InterfaceObjectsMap ret;
    auto interface = getDbusInterface();
    if (interface.empty() == false)
    {
        std::vector<std::string> list;
        auto deviceList = util::expandDeviceRange(getDbusObjectPath());
        for (const auto& device : deviceList)
        {
            list.push_back(device.second);
        }
        ret[interface] = list;
    }
    return ret;
}

bool DataAccessor::readDbus()
{
    clearData();
    bool ret = isValidDbusAccessor();
    if (ret == true)
    {
        /*
         * readDbusProperty() returns empty variant for the following cases:
         *     1. bad return from getService() -> empty service
         *     2. object path with range specification
         * readDbusProperty() forwards an exception when receives it from Dbus
         */
        auto propVariant = dbus::readDbusProperty(
            _acc[objectKey], _acc[interfaceKey], _acc[propertyKey]);
        // setDataValueFromVariant returns false in case variant is invalid
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
        uint64_t processExitCode = 0;
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
            processExitCode = static_cast<uint64_t>(process.exit_code());
            if (processExitCode != 0)
            {
                ret = false;
                std::string error("return code = ");
                error += std::to_string(processExitCode);
                std::cerr << "[E]"
                          << "Error running the command \'" << cmd << "\' "
                          << error << std::endl;
            }
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
            _dataValue = std::make_shared<PropertyValue>(result);
        }
    }
    return ret;
}

bool DataAccessor::setDataValueFromVariant(const PropertyVariant& propVariant)
{
    clearData();
    bool ret = isValidVariant(propVariant);
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

void DataAccessor::buildSingleAssertedDeviceName(
    DataAccessor& accData, const std::string& realDevice,
    const std::string& devType) const
{
    auto deviceName = util::determineDeviceName(realDevice, devType);
    if (deviceName.empty() == false)
    {
        util::DeviceIdMap map;
        map[0] = deviceName;
        accData._latestAssertedDevices = map;
    }
}

util::RangeInformation
    DataAccessor::getCmdLineRangeInformationInArguments() const
{
    if (isTypeCmdline() == true && _acc.count(argumentsKey) != 0)
    {
        return util::getRangeInformation(_acc[argumentsKey].get<std::string>());
    }
    // returns empty information
    return util::getRangeInformation(std::string{""});
}

} // namespace data_accessor
