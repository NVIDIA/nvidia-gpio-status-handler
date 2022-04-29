
/*
 *
 */
#include "data_accessor.hpp"

#include <fmt/format.h>

#include <boost/process.hpp>
#include <phosphor-logging/elog.hpp>

#include <regex>
#include <string>

namespace data_accessor
{

namespace dbus
{
using namespace phosphor::logging;

static auto bus = sdbusplus::bus::new_default();

std::string getService(const std::string& objectPath,
                       const std::string& interface)
{
    constexpr auto mapperBusBame = "xyz.openbmc_project.ObjectMapper";
    constexpr auto mapperObjectPath = "/xyz/openbmc_project/object_mapper";
    constexpr auto mapperInterface = "xyz.openbmc_project.ObjectMapper";
    std::vector<std::pair<std::string, std::vector<std::string>>> response;
    auto method = bus.new_method_call(mapperBusBame, mapperObjectPath,
                                      mapperInterface, "GetObject");
    method.append(objectPath, std::vector<std::string>({interface}));
    try
    {
        auto reply = bus.call(method);
        reply.read(response);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        log<level::ERR>("D-Bus call exception",
                        entry("OBJPATH={%s}", objectPath.c_str()),
                        entry("INTERFACE={%s}", interface.c_str()),
                        entry("SDBUSERR=%s", e.what()));

        throw std::runtime_error("Service name is not found");
    }

    if (response.empty())
    {
        throw std::runtime_error("Service name response is empty");
    }
    return response.begin()->first;
}

auto deviceGetCoreAPI(const int devId, const std::string& property)
{
    constexpr auto service = "xyz.openbmc_project.GpuMgr";
    constexpr auto object = "/xyz/openbmc_project/GpuMgr";
    constexpr auto interface = "xyz.openbmc_project.GpuMgr.Server";
    constexpr auto callName = "DeviceGetData";

    constexpr auto accMode = 1; // Calling in Passthrough Mode. Blocked call.

    auto method = bus.new_method_call(service, object, interface, callName);
    method.append(devId);
    method.append(property);
    method.append(accMode);

    std::tuple<int, std::string, std::vector<uint32_t>> response;
    try
    {
        auto reply = bus.call(method);
        reply.read(response);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>("Failed to make call for ",
                        entry("property=(%s[%d])", property.c_str(), devId),
                        entry("SDBUSERR=%s", e.what()));
        throw std::runtime_error("Get device call is not found!");
    }

    uint64_t value = 0;
    std::string valueStr = "";

    // response example:
    // (isau) 0 "Baseboard GPU over temperature info : 0001" 2 1 0
    auto rc = std::get<int>(response);

    if (rc != 0)
    {
        log<level::ERR>("Failed to get value of ",
                        entry("property=(%s[%d])", property.c_str(), devId),
                        entry("rc=%d", rc));
    }
    else
    {
        auto data = std::get<std::vector<uint32_t>>(response);
        // Per SMBPBI spec: data[0]:dataOut, data[1]:exDataOut
        value = ((uint64_t)data[1] << 32 | data[0]);

        // msg example: "Baseboard GPU over temperature info : 0001"
        auto msg = std::get<std::string>(response);
        valueStr = msg.substr(msg.find(" : "), msg.length());
    }

    return std::make_tuple(rc, valueStr, value);
}

auto deviceClearCoreAPI(const int devId, const std::string& property)
{
    constexpr auto service = "xyz.openbmc_project.GpuMgr";
    constexpr auto object = "/xyz/openbmc_project/GpuMgr";
    constexpr auto interface = "xyz.openbmc_project.GpuMgr.Server";
    constexpr auto callName = "DeviceClearData";

    auto method = bus.new_method_call(service, object, interface, callName);
    method.append(devId);
    method.append(property);

    int rc = 0;
    try
    {
        auto reply = bus.call(method);
        reply.read(rc);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>("Failed to make call for ",
                        entry("property=(%s[%d])", property.c_str(), devId),
                        entry("SDBUSERR=%s", e.what()));
        throw std::runtime_error("Clear device call is not found!");
    }

    if (rc != 0)
    {
        log<level::ERR>("Failed to get value of ",
                        entry("property=(%s[%d])", property.c_str(), devId),
                        entry("rc=%d", rc));
    }

    return rc;
}

} // namespace dbus

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
    if (_acc.count(checkKey) != 0)
    {
        DataAccessor& accNonConst = const_cast<DataAccessor&>(otherAcc);
        // until this moment device is not necessary in PropertyValue::check()
        accNonConst.read(device);
        if ((ret = accNonConst.hasData()) == true)
        {
            auto checkDefinition = _acc[checkKey].get<CheckDefinitionMap>();
            ret = otherAcc._dataValue->check(checkDefinition, redefCriteria);
        }
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

bool DataAccessor::readDbus()
{
    clearData();
    bool ret = isValidDbusAccessor();
    if (ret == true)
    {
        auto propVariant = readDbusProperty(_acc[objectKey], _acc[interfaceKey],
                                            _acc[propertyKey]);
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
            auto regexPosition = std::string::npos;
            const std::regex reg{".*(\\[[0-9]+\\-[0-9]+\\]).*"};
            std::smatch match;
            if (std::regex_search(args, match, reg))
            {
                std::string regexString = match[1];
                regexPosition = args.find(regexString);
                // replace the range by device
                if (device.empty() == false)
                {
                    args.replace(regexPosition, regexString.size(), device);
                }
                else // expand the range inside args
                {
                    // TODO: "before A[0-3] after" => "before A0 A1 A2 A3 after"
                    // args = expandRangeFromString(args);
                }
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

} // namespace data_accessor
