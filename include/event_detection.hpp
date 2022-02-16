
/*
 *
 */

#pragma once

#include "event_info.hpp"
#include <boost/container/flat_map.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <string>

namespace event_detection
{

enum class Status : int
{
    succ,
    error,
    timeout,
};

/** @class EventDetection
 *  @brief Responsible for catching D-Bus signals and GPIO state change
 * conditions.
 */
class EventDetection
{

  private:
    event_info::EventMap eventMap;
    
  public:
    EventDetection();
    ~EventDetection() = default;
    EventDetection(event_info::EventMap& eventMap) :
        eventMap(eventMap)
    {}
    std::unique_ptr<sdbusplus::bus::match_t> startEventDetection(
        std::shared_ptr<sdbusplus::asio::connection> conn,
        std::shared_ptr<sdbusplus::asio::dbus_interface> iface);
    void identifyEventCandidate(const std::string& objPath,
                                const std::string& signature,
                                const std::string& property);
    // EventDetection(std::shared_ptr<sdbusplus::asio::connection> conn);
};

} // namespace event_detection
