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

static std::string errorMsg(const std::string& description,
                            const std::string& objpath,
                            const std::string& interface,
                            const std::string& property = std::string{""},
                            const char* eWhat = nullptr)
{
    std::string msg{"[E] "};
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
    std::string ret{""};
    try
    {
        auto reply = bus.call(method);
        reply.read(response);
        if (response.empty() == false)
        {
            ret = response.begin()->first;
        }
        else
        {
            std::cerr << errorMsg("getService(): Service not found for",
                                  objectPath, interface)
                      << std::endl;
        }
    }
    catch (const sdbusplus::exception::exception& e)
    {
        std::cerr << errorMsg("getService(): DBus error for", objectPath,
                              interface, e.what())
                  << std::endl;
    }
    return ret;
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

    uint64_t value = 0;
    std::string valueStr = "";
    std::tuple<int, std::string, std::vector<uint32_t>> response;
    try
    {
        auto reply = bus.call(method);
        reply.read(response);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        std::cerr << errorMsg("deviceGetCoreAPI(): DBus error for",
                              std::string{""}, std::string{""}, interface,
                              e.what())
                  << std::endl;

        return std::make_tuple(-1, valueStr, value);
    }

    // response example:
    // (isau) 0 "Baseboard GPU over temperature info : 0001" 2 1 0
    auto rc = std::get<int>(response);

    if (rc != 0)
    {
        std::cerr << errorMsg("deviceGetCoreAPI(): bad return for",
                              std::string{""}, std::string{""}, object,
                              interface)
                  << std::endl;
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

    int rc = -1;
    std::string dbusError{""};
    try
    {
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
        std::cerr << errorMsg(msg, std::string{""}, std::string{""}, property,
                              dbusError.c_str())
                  << std::endl;
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
        std::cerr << errorMsg("readDbusProperty(): PATH with range", objPath,
                              interface, property)
                  << std::endl;
        return value;
    }

    auto service = getService(objPath, interface);
    if (service.empty() == true)
    {
        // getService() already printed error message
        return value;
    }

    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto method = bus.new_method_call(service.c_str(), objPath.c_str(),
                                          freeDesktopInterface, getCall);
        method.append(interface, property);
        auto reply = bus.call(method);
        reply.read(value);
    }
    catch (const std::exception& e)
    {
        std::cerr << errorMsg("readDbusProperty() Failed to get property",
                              objPath, interface, property, e.what())
                  << std::endl;
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