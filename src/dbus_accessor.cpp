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

#include "util.hpp"

#include <phosphor-logging/elog.hpp>
#include <sdbusplus/exception.hpp>

namespace data_accessor
{

namespace dbus
{

using namespace phosphor::logging;

std::string getService(const std::string& objectPath,
                       const std::string& interface)
{
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

    std::vector<std::pair<std::string, std::vector<std::string>>> response;
    auto bus = sdbusplus::bus::new_default();
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

RetCoreApi deviceGetCoreAPI(const int devId, const std::string& property)
{
    constexpr auto service = "xyz.openbmc_project.GpuMgr";
    constexpr auto object = "/xyz/openbmc_project/GpuMgr";
    constexpr auto interface = "xyz.openbmc_project.GpuMgr.Server";
    constexpr auto callName = "DeviceGetData";

    constexpr auto accMode = 1; // Calling in Passthrough Mode. Blocked call.

    auto bus = sdbusplus::bus::new_default();
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
        std::string msg{callName};
        msg += "(\'" + property + "\') Failed to make call";
        std::cout << " " << msg;
        std::cout << std::endl;
        std::cout << std::endl;
        log<level::ERR>(msg.c_str(),
                        entry("property=(%s[%d])", property.c_str(), devId),
                        entry("SDBUSERR=%s", e.what()));

        throw std::runtime_error(msg);
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
        valueStr = std::get<std::string>(response);
    }

    return std::make_tuple(rc, valueStr, value);
}

int deviceClearCoreAPI(const int devId, const std::string& property)
{
    constexpr auto service = "xyz.openbmc_project.GpuMgr";
    constexpr auto object = "/xyz/openbmc_project/GpuMgr";
    constexpr auto interface = "xyz.openbmc_project.GpuMgr.Server";
    constexpr auto callName = "DeviceClearData";

    auto bus = sdbusplus::bus::new_default();
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

PropertyVariant readDbusProperty(const std::string& objPath,
                                 const std::string& interface,
                                 const std::string& property)
{

    PropertyVariant value;
    if (util::existsRange(objPath) == true)
    {
        log<level::ERR>("PATH is invalid, with has range specification ",
                        entry("path=(%s)", objPath.c_str()),
                        entry("interface=(%s)", interface.c_str()),
                        entry("property=(%s)", property.c_str()));
        return value;
    }

    try
    {
        auto service = getService(objPath, interface);
        auto bus = sdbusplus::bus::new_default();
        auto method = bus.new_method_call(service.c_str(), objPath.c_str(),
                                          freeDesktopInterface, getCall);
        method.append(interface, property);
        auto reply = bus.call(method);
        reply.read(value);
    }
    // sdbusplus::exception::SdBusError inherits std::exception
    catch (const std::exception& error)
    {
        log<level::ERR>("Failed to get property ",
                        entry("path=(%s)", objPath.c_str()),
                        entry("interface=(%s)", interface.c_str()),
                        entry("property=(%s)", property.c_str()),
                        entry("error: %s", error.what()));
        throw error;
    }
    return value;
}

DbusPropertyChangedHandler
    registerServicePropertyChanged(DbusAsioConnection conn,
                                   const std::string& service,
                                   CallbackFunction callback)
{
    return registerServicePropertyChanged(
        static_cast<sdbusplus::bus::bus&>(*conn), service, callback);
}

DbusPropertyChangedHandler
    registerServicePropertyChanged(sdbusplus::bus::bus& bus,
                                   const std::string& service,
                                   CallbackFunction callback)
{
    auto lambdaCallbackHandler = [service,
                                  callback](sdbusplus::message::message& msg) {
        if (getService(msg.get_path(), msg.get_interface()) == service)
        {
#ifdef ENABLE_LOGS
            std::cout << "registerServicePropertyChanged()"
                      << " calling service:" << service
                      << "\n\t path:" << msg.get_path()
                      << " interface:" << msg.get_interface() << std::endl;
#endif
            callback(msg);
        }
#ifdef ENABLE_LOGS
        else
        {
            std::cout << "registerServicePropertyChanged()"
                      << " service:" << service
                      << "\n\tdiscarding path:" << msg.get_path()
                      << " interface:" << msg.get_interface() << std::endl;
        }
#endif
    };
    DbusPropertyChangedHandler propertyHandler =
        std::make_unique<sdbusplus::bus::match_t>(
            bus,
            sdbusplus::bus::match::rules::type::signal() +
                sdbusplus::bus::match::rules::sender(service) +
                sdbusplus::bus::match::rules::member("PropertiesChanged") +
                sdbusplus::bus::match::rules::interface(
                    "org.freedesktop.DBus.Properties"),
            std::move(lambdaCallbackHandler));

    return propertyHandler;
}

} // namespace dbus

} // namespace data_accessor
