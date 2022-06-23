/*
 Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.

 NVIDIA CORPORATION and its licensors retain all intellectual property
 and proprietary rights in and to this software, related documentation
 and any modifications thereto.  Any use, reproduction, disclosure or
 distribution of this software and related documentation without an express
 license agreement from NVIDIA CORPORATION is strictly prohibited.
*
*/

#pragma once

#include "property_accessor.hpp"

#include <nlohmann/json.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus.hpp>

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <tuple>
#include <vector>

namespace dbus
{

constexpr auto freeDesktopInterface = "org.freedesktop.DBus.Properties";
constexpr auto getCall = "Get";
constexpr auto setCall = "Set";

using DbusPropertyChangedHandler = std::unique_ptr<sdbusplus::bus::match_t>;
using CallbackFunction = sdbusplus::bus::match::match::callback_t;
using DbusAsioConnection = std::shared_ptr<sdbusplus::asio::connection>;

/**
 * @brief register for receiving signals from Dbus PropertyChanged
 *
 * @param conn        connection std::shared_ptr<sdbusplus::asio::connection>
 * @param objectPath  Dbus object path
 * @param interface   Dbus interface
 * @param callback    the callback function
 * @return the match_t register information which cannot be destroyed
 *         while receiving these Dbus signals
 */
DbusPropertyChangedHandler
    registerServicePropertyChanged(DbusAsioConnection conn,
                                   const std::string& objectPath,
                                   const std::string& interface,
                                   CallbackFunction callback);

/**
 * @brief overloaded function
 * @param bus       the bus type sdbusplus::bus::bus&
 * @param objectPath
 * @param interface
 * @param callback
 * @return
 */
DbusPropertyChangedHandler
    registerServicePropertyChanged(sdbusplus::bus::bus& bus,
                                   const std::string& objectPath,
                                   const std::string& interface,
                                   CallbackFunction callback);

/**
 *  @brief this is the return type for @sa deviceGetCoreAPI()
 */
using RetCoreApi = std::tuple<int, std::string, uint64_t>; // int = return code

/**
 * @brief returns the service assigned with objectPath and interface
 * @param objectPath
 * @param interface
 * @return service name
 */
std::string getService(const std::string& objectPath,
                       const std::string& interface);

/**
 * @brief Performs a DBus call in GpuMgr service calling DeviceGetData method
 * @param devId
 * @param property  the name of the property which defines which data to get
 * @return RetCoreApi with the information
 */
RetCoreApi deviceGetCoreAPI(const int devId, const std::string& property);

/**
 * @brief clear information present GpuMgr service DeviceGetData method
 * @param devId
 * @param property
 * @return 0 meaning success or other value to indicate an error
 */
int deviceClearCoreAPI(const int devId, const std::string& property);

/**
 * @brief getDbusProperty() gets the value from a property in DBUS
 * @param objPath
 * @param interface
 * @param property
 * @return the value based on std::variant
 */
PropertyVariant readDbusProperty(const std::string& objPath,
                                 const std::string& interface,
                                 const std::string& property);

/**
 * @brief setDbusProperty() sets a value for a Dbus property
 * @param service
 * @param objPath
 * @param interface
 * @param property
 * @param val the new value to be set
 * @return true if could set this the value from 'val', false otherwise
 */
bool setDbusProperty(const std::string& service, const std::string& objPath,
                     const std::string& interface, const std::string& property,
                     const PropertyVariant& val);

/**
 * @brief setDbusProperty() just an overload function that calls getService()
 *                          to get the service for objPath and interface
 * @param objPath
 * @param interface
 * @param property
 * @param val the new value to be set
 * @return true if could set this the value from 'val', false otherwise
 */
bool setDbusProperty(const std::string& objPath, const std::string& interface,
                     const std::string& property, const PropertyVariant& val);

} // namespace dbus
