/*
 Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.

 NVIDIA CORPORATION and its licensors retain all intellectual property
 and proprietary rights in and to this software, related documentation
 and any modifications thereto.  Any use, reproduction, disclosure or
 distribution of this software and related documentation without an express
 license agreement from NVIDIA CORPORATION is strictly prohibited.
*
*/

#include "dbus_accessor.hpp"

#include "log.hpp"
#include "util.hpp"

#include <boost/algorithm/string.hpp>
#include <phosphor-logging/elog.hpp>
#include <sdbusplus/exception.hpp>

namespace dbus
{

using namespace phosphor::logging;

static std::string errorMsg(const std::string& description,
                            const std::string& objpath,
                            const std::string& interface,
                            const std::string& property = std::string{""},
                            const char* eWhat = nullptr)
{
    std::string msg{};
    msg += description;
    if (objpath.empty() == false)
    {
        msg += " Objectpath=" + objpath;
    }
    if (interface.empty() == false)
    {
        msg += " Interface=" + interface;
    }
    if (property.empty() == false)
    {
        msg += " Property=" + property;
    }
    if (eWhat != nullptr)
    {
        msg += " Error: ";
        msg += eWhat;
    }
    return msg;
}

std::string getService(const std::string& objectPath,
                       const std::string& interface)
{
    log_elapsed();
    constexpr auto mapperBusBame = "xyz.openbmc_project.ObjectMapper";
    constexpr auto mapperObjectPath = "/xyz/openbmc_project/object_mapper";
    constexpr auto mapperInterface = "xyz.openbmc_project.ObjectMapper";

    /**
     * it looks like ObjectMapper does not know GpioStatusHandler service
     */
#if 1
    constexpr auto gpioStatusService = "xyz.openbmc_project.GpioStatusHandler";
    if (objectPath.find("GpioStatusHandler") != std::string::npos)
    {
        return gpioStatusService;
    }
#endif

    std::string ret{""};
    std::vector<std::pair<std::string, std::vector<std::string>>> response;
    auto bus = sdbusplus::bus::new_default();
    try
    {
        auto method = bus.new_method_call(mapperBusBame, mapperObjectPath,
                                          mapperInterface, "GetObject");
        method.append(objectPath, std::vector<std::string>({interface}));
        auto reply = bus.call(method);
        reply.read(response);
        if (response.empty() == false)
        {
            ret = response.begin()->first;
        }
        else
        {
            std::string tmp = errorMsg("getService(): Service not found for",
                                       objectPath, interface);
            logs_err("%s\n", tmp.c_str());
        }
    }
    catch (const sdbusplus::exception::exception& e)
    {
        std::string tmp = errorMsg("getService(): DBus error for", objectPath,
                                   interface, e.what());
        logs_err("%s\n", tmp.c_str());
    }
    return ret;
}

RetCoreApi deviceGetCoreAPI(const int devId, const std::string& property)
{
    log_elapsed();
    constexpr auto service = "xyz.openbmc_project.GpuMgr";
    constexpr auto object = "/xyz/openbmc_project/GpuMgr";
    constexpr auto interface = "xyz.openbmc_project.GpuMgr.Server";
    constexpr auto callName = "DeviceGetData";
    constexpr auto accMode = 1; // Calling in Passthrough Mode. Blocked call.

    uint64_t value = 0;
    std::string valueStr = "";
    std::tuple<int, std::string, std::vector<uint32_t>> response;
    auto bus = sdbusplus::bus::new_default();
    try
    {
        auto method = bus.new_method_call(service, object, interface, callName);
        method.append(devId);
        method.append(property);
        method.append(accMode);
        auto reply = bus.call(method);
        reply.read(response);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        std::string tmp =
            errorMsg("deviceGetCoreAPI(): DBus error for", std::string{""},
                     std::string{""}, property, e.what());
        logs_err("%s\n", tmp.c_str());

        return std::make_tuple(-1, valueStr, value);
    }

    // response example:
    // (isau) 0 "Baseboard GPU over temperature info : 0001" 2 1 0
    auto rc = std::get<int>(response);

    if (rc != 0)
    {
        std::string tmp = errorMsg("deviceGetCoreAPI(): bad return for",
                                   std::string{""}, std::string{""}, property);
        logs_err("%s rc=%ld\n", tmp.c_str(), rc);
    }
    else
    {
        auto data = std::get<std::vector<uint32_t>>(response);
        if (data.size() >= 2)
        {
            // Per SMBPBI spec: data[0]:dataOut, data[1]:exDataOut
            value = ((uint64_t)data[1] << 32 | data[0]);
        }

        // msg example: "Baseboard GPU over temperature info : 0001"
        valueStr = std::get<std::string>(response);
    }
    logs_dbg("devId: %d property: %s; rc=%ld value=%llu string='%s'\n", devId,
             property.c_str(), rc, value, valueStr.c_str());

    return std::make_tuple(rc, valueStr, value);
}

int deviceClearCoreAPI(const int devId, const std::string& property)
{
    log_elapsed();
    constexpr auto service = "xyz.openbmc_project.GpuMgr";
    constexpr auto object = "/xyz/openbmc_project/GpuMgr";
    constexpr auto interface = "xyz.openbmc_project.GpuMgr.Server";
    constexpr auto callName = "DeviceClearData";

    int rc = -1;
    std::string dbusError{""};
    auto bus = sdbusplus::bus::new_default();
    try
    {
        auto method = bus.new_method_call(service, object, interface, callName);
        method.append(devId);
        method.append(property);
        auto reply = bus.call(method);
        reply.read(rc);
    }
    catch (const sdbusplus::exception::SdBusError& error)
    {
        if (rc == 0)
        {
            rc = -1; // just in case reply.read() failed but set rc = 0
        }
        dbusError = " DBus failed, ";
        dbusError += error.what();
    }

    if (rc != 0)
    {
        std::string msg{"deviceClearCoreAPI() Failed "};
        msg += "devId:" + std::to_string(devId);
        std::string tmp = errorMsg(msg, std::string{""}, std::string{""},
                                   property, dbusError.c_str());
        logs_err("%s\n", tmp.c_str());
    }
    logs_dbg("rc=%d property='%s' devId=%d\n", rc, property.c_str(), devId);
    return rc;
}

PropertyVariant readDbusProperty(const std::string& objPath,
                                 const std::string& interface,
                                 const std::string& property)
{
    log_elapsed();
    PropertyVariant value;
    if (util::existsRange(objPath) == true)
    {
        std::string tmp = errorMsg("readDbusProperty(): PATH with range",
                                   objPath, interface, property);
        logs_err("%s\n", tmp.c_str());
        return value;
    }

    auto service = getService(objPath, interface);
    if (service.empty() == true)
    {
        // getService() already printed error message
        return value;
    }
    auto bus = sdbusplus::bus::new_default();
    try
    {
        auto method = bus.new_method_call(service.c_str(), objPath.c_str(),
                                          freeDesktopInterface, getCall);
        method.append(interface, property);
        auto reply = bus.call(method);
        reply.read(value);
        auto valueStr = data_accessor::PropertyValue(value).getString();
        logs_dbg("object=%s \n\tinterface=%s property=%s value=%s\n",
                 objPath.c_str(), interface.c_str(), property.c_str(),
                 valueStr.c_str());
    }
    catch (const sdbusplus::exception::exception& e)
    {
        std::string tmp = errorMsg("readDbusProperty() Failed to get property",
                                   objPath, interface, property, e.what());
        logs_err("%s\n", tmp.c_str());
    }
    return value;
}

DbusPropertyChangedHandler registerServicePropertyChanged(
    DbusAsioConnection conn, const std::string& objectPath,
    const std::string& interface, CallbackFunction callback)
{
    return registerServicePropertyChanged(
        static_cast<sdbusplus::bus::bus&>(*conn), objectPath, interface,
        callback);
}

DbusPropertyChangedHandler registerServicePropertyChanged(
    sdbusplus::bus::bus& bus, const std::string& objectPath,
    const std::string& interface, CallbackFunction callback)
{
    log_elapsed();
    DbusPropertyChangedHandler propertyHandler;
    try
    {
        auto subscribeStr = sdbusplus::bus::match::rules::propertiesChanged(
            objectPath, interface);
        logs_dbg("subscribeStr: %s\n", subscribeStr.c_str());
        propertyHandler = std::make_unique<sdbusplus::bus::match_t>(
            bus, subscribeStr, callback);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        std::string tmp = errorMsg("registerServicePropertyChanged(): Error",
                                   objectPath, interface, "", e.what());
        logs_err("%s\n", tmp.c_str());
        throw std::runtime_error(e.what());
    }
    return propertyHandler;
}

bool setDbusProperty(const std::string& objPath, const std::string& interface,
                     const std::string& property, const PropertyVariant& val)
{
    return setDbusProperty(getService(objPath, interface), objPath, interface,
                           property, val);
}

bool setDbusProperty(const std::string& service, const std::string& objPath,
                     const std::string& interface, const std::string& property,
                     const PropertyVariant& val)
{
    log_elapsed();
    auto bus = sdbusplus::bus::new_default();
    bool ret = false;
    try
    {
        auto method = bus.new_method_call(service.c_str(), objPath.c_str(),
                                          freeDesktopInterface, setCall);
        method.append(interface, property, val);
        ret = true;
        if (!bus.call(method))
        {
            ret = false;
        }
    }
    catch (const sdbusplus::exception::exception& e)
    {
        std::string tmp = errorMsg("setDbusProperty() Failed to set property",
                                   objPath, interface, property, e.what());
        logs_err("%s\n", tmp.c_str());
    }
    return ret;
}

std::vector<std::string> ObjectMapper::scopeObjectPathsDevId(
    const std::vector<std::string>& objectPaths, const std::string& devId)
{
    // marcinw:TODO: use std
    std::vector<std::string> result;
    for (const auto& objPath : objectPaths)
    {
        if (boost::algorithm::ends_with(objPath, "/" + devId) ||
            boost::algorithm::ends_with(objPath, "/HGX_" + devId))
        {
            result.push_back(objPath);
        }
    }
    return result;
}

} // namespace dbus
