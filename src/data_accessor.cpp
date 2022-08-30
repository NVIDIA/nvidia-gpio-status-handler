/*
 *
 */
#include "data_accessor.hpp"

#include "event_info.hpp"
#include "log.hpp"

#include <boost/process.hpp>

#include <regex>
#include <string>
#include <vector>

namespace data_accessor
{

bool DataAccessor::contains(const DataAccessor& other) const
{
    bool ret = isValid(other._acc);
    std::stringstream ss;
    if (ret == true)
    {
        for (auto& [key, val] : other._acc.items())
        {
            if (_acc.count(key) == 0)
            {
                ss << "key=" << key << " not found in _acc";
                log_dbg("%s\n", ss.str().c_str());
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
                ss.str(std::string()); // Clearing the stream first
                ss << "values do not match key=" << key << " value=" << val;
                log_dbg("%s\n", ss.str().c_str());
                ret = false;
                break;
            }
        }
    }
    ss.str(std::string()); // Clearing the stream first
    ss << "ret=" << ret;
    log_dbg("%s\n", ss.str().c_str());
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
         * Note:
         *   As read(device) is for a single device, there are cases where
         *   it is necessary to loop all devices calling read() and subCheck()
         *   These cases are:
         *   1. CMDLINE i.g., "arguments": "AP0_BOOTCOMPLETE_TIMEOUT GPU[0-7]",
         *   2. DeviceCoreAPI having range in event 'device_type', not having
         *      “device_id” field or having “device_id” equal “range”
         */
        util::DeviceIdMap devRange{};
        if (isTypeCmdline() == true)
        {
            devRange = getCmdLineRangeArguments(deviceType); // case 1
        }
        else if (isTypeDeviceCoreApi() == true && isDeviceIdRange() == true)
        {
            // working so far with 'equal' and 'bitmask'
            devRange = util::expandDeviceRange(deviceType); // case 2
        }
        if (devRange.size() > 0)
        {
            auto tmpAccessor = *this;
            ret = tmpAccessor.checkLoopingDevices(devRange, otherAcc,
                                                  redefCriteria, deviceType);
        }
        else // not necessary to loop all devices, using orignal deviceToRead
        {
            accNonConst.read(deviceToRead);
            ret = subCheck(otherAcc, redefCriteria, deviceType, deviceToRead);
        }
    }
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
                            const std::string& dev2Read) const
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
    std::stringstream ss;
    ss << "ret=" << ret;
    log_dbg("%s\n", ss.str().c_str());
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
        std::stringstream ss;
        ss << "cmd: " << cmd;
        log_dbg("%s\n", ss.str().c_str());
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
                ss.str(std::string()); // Clear the stream
                ss << "Error running the command \'" << cmd << "\' " << error;
                log_err("%s\n", ss.str().c_str());
            }
        }
        catch (const std::exception& error)
        {
            ret = false;
            ss.str(std::string()); // Clear the stream
            ss << "Error running the command \'" << cmd << "\' "
               << error.what();
            log_err("%s\n", ss.str().c_str());
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
            if (existsCheckLookup() == true && isDeviceIdRange() == false)
            {
                _dataValue = std::make_shared<PropertyValue>(PropertyValue(
                    std::get<std::string>(tuple), std::get<uint64_t>(tuple)));
            }
            else
            {
                _dataValue = std::make_shared<PropertyValue>(
                    PropertyValue(std::get<uint64_t>(tuple)));
            }
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

util::DeviceIdMap
    DataAccessor::getCmdLineRangeArguments(const std::string& deviceType) const
{
    util::DeviceIdMap devArgs;
    if (isValidCmdlineAccessor() == true && _acc.count(argumentsKey) != 0)
    {
        auto args = _acc[argumentsKey].get<std::string>();
        auto cmdLineRange = util::getRangeInformation(args);
        auto sizeRangeString = std::get<0>(cmdLineRange);
        if (sizeRangeString > 0)
        {
            /** check for empty brackets like {"arguments", "AP0_BOOT []"},
             *  size will be 2 even there were spaces (were ignored) between the
             *  brackets, then use deviceType which must have range
             */
            if (sizeRangeString == 2 && util::existsRange(deviceType))
            {
                devArgs = util::expandDeviceRange(deviceType);
            }
            else
            {
                devArgs = util::expandDeviceRange(std::get<2>(cmdLineRange));
            }
        }
    }
    return devArgs;
}

bool DataAccessor::checkLoopingDevices(const util::DeviceIdMap& devices,
                                       const DataAccessor& otherAcc,
                                       const PropertyVariant& redefCriteria,
                                       const std::string& deviceType)
{
    bool ret = false;
    DataAccessor& accNonConst = const_cast<DataAccessor&>(otherAcc);
    for (auto& arg : devices)
    {
        read(arg.second); // arg.first=deviceId, arg.second=deviceName
        if (subCheck(*this, redefCriteria, deviceType, arg.second) == true)
        {
            ret = true;
            // insert the asserteDeviceName from tmpAccessor into
            //  return map, it always comes as devId 0
            if (_latestAssertedDevices.count(0) != 0)
            {
                accNonConst._latestAssertedDevices[arg.first] =
                    _latestAssertedDevices.at(0);
            }
        }
    }
    return ret;
}

std::string DataAccessor::read(const event_info::EventNode& event)
{
    if (isTypeDeviceName() || isTypeDeviceCoreApi() || isTypeCmdline())
    {
        // just device name, eg. "GPU1", "NVSwitch0", etc
        return read(event.device);
    }
    else if (isTypeDbus() || isTypeDbus() || isTypeDevice() || isTypeTest() ||
             isTypeConst())
    {
        // 'read' doesn't use the 'device' argument for those types
        // apparently
        return read();
    }
    else
    {
        throw std::runtime_error(
            "Unrecognised data accessor type. "
            "If a new accessor type was added a change in '" __FILE__
            ":getStringMessageArg' is needed");
    }
}

} // namespace data_accessor
