
/*
 *
 */

#pragma once

#include <string>
#include <boost/container/flat_map.hpp>
#include <sdbusplus/asio/object_server.hpp>

namespace event_detection
{

enum class Status : int
{
  succ,
  error,
  timeout,
};

/** @class EventDetection
 *  @brief Responsible for catching D-Bus signals and GPIO state change conditions.
 */
class EventDetection
{
 
    
  public:
    EventDetection();
    ~EventDetection() = default;
    static std::unique_ptr<sdbusplus::bus::match_t> startEventDetection(std::shared_ptr<sdbusplus::asio::connection> conn,
                                                                        std::shared_ptr<sdbusplus::asio::dbus_interface> iface);
    //EventDetection(std::shared_ptr<sdbusplus::asio::connection> conn);

};

} // namespace event_detection
