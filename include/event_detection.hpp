
/*
 *
 */

#pragma once

#include "aml.hpp"
#include "event_handler.hpp"
#include "event_info.hpp"
#include "object.hpp"

#include <boost/container/flat_map.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <iostream>
#include <string>
#include <thread>

namespace event_detection
{

/**
 * @class EventDetection
 * @brief Responsible for catching D-Bus signals and GPIO state change
 * conditions.
 */
class EventDetection : public object::Object
{
  public:
    EventDetection(const std::string& name, event_info::EventMap* eventMap,
                   event_handler::EventHandlerManager* hdlrMgr) :
        object::Object(name),
        _eventMap(eventMap), _hdlrMgr(hdlrMgr)
    {}
    ~EventDetection() = default;

  public:
    void identifyEventCandidate(const std::string& objPath,
                                const std::string& signature,
                                const std::string& property);

    /**
     * @brief Register DBus propertiesChanged handler.
     *
     * @param evtDet
     * @param conn
     * @param iface
     * @return std::unique_ptr<sdbusplus::bus::match_t>
     */
    static std::unique_ptr<sdbusplus::bus::match_t>
        startEventDetection(EventDetection* evtDet,
                            std::shared_ptr<sdbusplus::asio::connection> conn);

    /**
     * @brief Lookup EventNode per the accessor info
     *
     * @param acc
     * @return event_info::EventNode*
     */
    event_info::EventNode*
        LookupEventFrom(const data_accessor::DataAccessor& acc)
    {
        for (auto& eventPerDevType : *this->_eventMap)
        {
            for (auto& event : eventPerDevType.second)
            {
                // event.accessor["object"] =
                // "xyz/openbmc_project/sensors/temperature/TEMP_GB_GPU[0-7]"
                // acc["object"]

                std::cout << "event.accessor: " << event.accessor
                          << ", acc: " << acc << "\n";
                if (event.accessor == acc)
                {
                    return &event;
                }
            }
        }
        return nullptr;
    }

    /**
     * @brief Check if the candidate is an event based on Leaky Bucket logic.
     *
     * @param candidate
     * @return true
     * @return false
     */
    bool IsEvent(event_info::EventNode& candidate)
    {
        auto countDiff = candidate.triggerCount - (candidate.count[candidate.device] + 1); 

        if (countDiff <= 0){
            candidate.count[candidate.device] = candidate.triggerCount;
            return true;
        }

        candidate.count[candidate.device]++;
        return false;
    }

    /**
     * @brief Throw out a thread to run all event handlers on the Event.
     *
     * @param event
     */
    void RunEventHandlers(event_info::EventNode& event)
    {
        std::cout << "Create thread to process event.\n";
        auto thread = new std::thread([this, event]() mutable {
            std::cout << "calling hdlrMgr: " << this->_hdlrMgr->getName()
                      << "\n";
            auto hdlrMgr = *this->_hdlrMgr;
            hdlrMgr.RunAllHandlers(event);
        });

        if (thread == nullptr)
        {
            std::cout << "Create thread to process event failed!\n";
        }
    }

    /**
     * @brief Determine device name from DBus object path.
     *
     * @param objPath
     * @param devType
     * @return std::string
     */
    static std::string DetermineDeviceName(const std::string& objPath,
                                           const std::string& devType)
    {
        const std::regex r{".*(" + devType + "[0-9]+).*"}; // TODO: fixme
        std::smatch m;

        if (std::regex_search(objPath.begin(), objPath.end(), m, r))
        {
            auto name = m[1]; // the 2nd field is the matched substring.
            std::cout << "Devname: " << name << "\n.";
            return name;
        }
        return "";
    }

  private:
    /**
     * @brief Pointer to the eventMap.
     *
     */
    event_info::EventMap* _eventMap;

    /**
     * @brief Pointer to the eventHanlderManager.
     *
     */
    event_handler::EventHandlerManager* _hdlrMgr;
};

} // namespace event_detection
