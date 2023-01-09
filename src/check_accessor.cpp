#include "check_accessor.hpp"

namespace data_accessor
{

std::string CheckAccessor::findDeviceName(const DataAccessor& jsonAcc,
                                          const DataAccessor& dataAcc,
                                          const std::string& deviceType) const
{
    auto objectName = dataAcc.getDbusObjectPath();
    std::string deviceName{""};
    if (objectName.empty() == false)
    {
        deviceName = util::determineDeviceName(objectName, deviceType);
        if (deviceName.empty() == true)
        {
            deviceName = util::getDeviceName(objectName);
        }
    }
    if (deviceName.empty() == true)
    {
        deviceName = util::getDeviceName(deviceType);
    }
    if (deviceName.empty() == true)
    {
        objectName = jsonAcc.getDbusObjectPath();
        if (objectName.empty() == false)
        {
            deviceName = util::getDeviceName(objectName);
        }
    }
    return deviceName;
}

bool
CheckAccessor::buildSingleAssertedDeviceName(const DataAccessor& dataAcc,
                                             const std::string& realDevice,
                                             const std::string& devType,
                                             const int deviceId)
{
    // if deviceId is valid means realDevice is already an asserted device
    auto deviceName = realDevice;
    bool ret = (deviceId != util::InvalidDeviceId && !deviceName.empty());
    if (ret == false)
    {
        deviceName = util::determineAssertedDeviceName(realDevice, devType);
        if (deviceName.empty() == true && util::existsRange(devType) == false)
        {
            // no reason to not consider 'device_type' as asserted device
            deviceName = devType;
        }
        if (deviceName.empty() == false)
        {
            ret = true;
        }
    }
    if (ret == true)
    {
        auto devId = deviceId;
        if (devId == -1)
        {
            devId = util::getDeviceId(deviceName, devType);
        }
        _assertedDevices.push_back(
                  AssertedDevice(_trigger, dataAcc, deviceName, devId));

    }
    return ret;
}

bool CheckAccessor::loopDevices(const util::DeviceIdMap& devices,
                                const DataAccessor& jsonAcc,
                                DataAccessor& dataAcc,
                                const std::string& deviceType)
{
    bool ret = false;
    for (auto& arg : devices)
    {
        // arg.first=deviceId, arg.second=deviceName
        const auto& deviceId = arg.first;
        const auto& deviceName = arg.second;
        dataAcc.read(deviceName);
        if (subCheck(jsonAcc, dataAcc, deviceType, deviceName, deviceId))
        {
            ret = true; // it at least one passes the entire check also passes
        }
    }
    return ret;
}

bool CheckAccessor::check(const DataAccessor& jsonAcc,
                          const DataAccessor& dataAcc,
                          const std::string& deviceType)
{
    auto tempAccData = dataAcc;
    return privCheck(jsonAcc, tempAccData, deviceType);
}

bool CheckAccessor::privCheck(const DataAccessor& jsonAcc,
                              DataAccessor& dataAcc,
                              const std::string& deviceType)
{
    bool ret = true; // defaults to true if accessor["check"] does not exist
    std::string deviceToRead = findDeviceName(jsonAcc, dataAcc, deviceType);
    _assertedDevices.clear();
    if (_trigger.isEmpty())
    {
        _trigger = dataAcc;
    }
    if (deviceToRead.empty() && false == _triggerAssertedDevice.empty())
    {
        deviceToRead = _triggerAssertedDevice;
    }
    // set the status here
    _lastStatus = NotPassed;
    if (jsonAcc.existsCheckKey() == true)
    {
        ret = false;
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
        if (true == deviceToRead.empty())
        {
            if (jsonAcc.isTypeCmdline() == true)
            {
                // case 1
                devRange = jsonAcc.getCmdLineRangeArguments(deviceType);
            }
            else if (jsonAcc.isTypeDeviceCoreApi() && jsonAcc.isDeviceIdRange())
            {
                // working so far with 'equal' and 'bitmask'
                devRange = util::expandDeviceRange(deviceType); // case 2
            }
        }
        if (devRange.size() > 0)
        {
            DataAccessor tempDataAcc = jsonAcc; // tempDataAcc will have data in
            ret = loopDevices(devRange, jsonAcc, tempDataAcc, deviceType);
        }
        else // not necessary to loop all devices, using original deviceToRead
        {
            if (dataAcc.isTypeDbus() == false || dataAcc.hasData() == false)
            {
                dataAcc.read(deviceToRead);
            }
            ret = subCheck(jsonAcc, dataAcc, deviceType, deviceToRead);
        }
    }
    else
    {
        // there is not "check" in the dataAcc, then needs to get devices
        buildSingleAssertedDeviceName(dataAcc, deviceToRead, deviceType);
    }
    if (ret == true)
    {
        _lastStatus = Passed;
    }

    // log stuff
    unsigned long size = _assertedDevices.size();
    log_dbg("asserted vector size=%lu\n", size);
    for (auto& deviceData : _assertedDevices)
    {
        log_dbg("Id=%d device=%s data=%s\n", deviceData.deviceId,
               deviceData.device.c_str(),
               deviceData.accessor.getDataValue().getString().c_str());
    }
    return ret;
}

bool CheckAccessor::check(const DataAccessor& jsonAcc,
                          const CheckAccessor& triggerCheck,
                          const std::string &deviceType)
{
    _trigger = triggerCheck._trigger;
    // this accessor will contains data if that passes
    auto accessorData = jsonAcc;
    // using trigger Check assertedDevice
    if (triggerCheck.hasAssertedDevices())
    {
        _triggerAssertedDevice = triggerCheck._assertedDevices.at(0).device;
    }
    privCheck(accessorData, accessorData, deviceType);

    // prevent the cases when:
    //   1.  event.trigger exists and event.accessor does NOT exist
    //   2.  could NOT build assertedDevice in this Check but triggerCheck could
    //          example for (2) is "DRAM Contained ECC Error"
    if (passed() && hasAssertedDevices() == false)
    {
        _assertedDevices = triggerCheck._assertedDevices;
        if (accessorData.hasData() == true)
        {
            for (auto& asserteDevice : _assertedDevices)
            {
                // trigger, device and deviceId are already there, sets accessor
                asserteDevice.accessor = accessorData;
            }
        }
    }
    return passed();
}

bool CheckAccessor::subCheck(const DataAccessor& jsonAcc,
                             DataAccessor& dataAcc,
                             const std::string& deviceType,
                             const std::string& dev2Read,
                             const int deviceId)
{
    if (dataAcc.hasData() == false)
    {
        return false; // without data nothing to do
    }
    bool ret = false;
    auto checkMap = jsonAcc[checkKey].get<CheckDefinitionMap>();
    /**
     * special logic for bitmask
     * --------------------------
     *   bitmask should match for deviceId present in either:
     *    1. deviceToRead and there is no regex in device
     *    2. device is regex, i.e, comes from 'event.device_type'
     */
    if (jsonAcc.existsCheckBitmap() == true)
    {
        PropertyValue bitmapValue;
        auto jsonBitmapValue = jsonAcc[checkKey][bitmapKey].get<std::string>();
        bitmapValue = PropertyValue(jsonBitmapValue);
        if (bitmapValue.isValidInteger() == true)
        {
            util::DeviceIdMap devices = util::expandDeviceRange(deviceType);
            // now walk thuru devices
            for (auto& deviceItem : devices)
            {
                auto devId = deviceItem.first;
                PropertyVariant bitmask(bitmapValue.getInteger() << devId);
                if (dataAcc.getDataValue().check(checkMap, bitmask) == true)
                {
                    ret = true;
                    buildSingleAssertedDeviceName(dataAcc, deviceItem.second,
                                                  deviceType, devId);
                }
            }
        }
    }
    else if (dataAcc.getDataValue().check(checkMap, PropertyVariant()))
    {
        ret = true;
        buildSingleAssertedDeviceName(dataAcc, dev2Read, deviceType, deviceId);
    }
    return ret;
}

} // namespace data_accessor
