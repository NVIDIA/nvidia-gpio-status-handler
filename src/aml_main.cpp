/**
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "aml_main.hpp"

#include "aml.hpp"
#include "aml_main.hpp"
#include "cmd_line.hpp"
#include "dat_traverse.hpp"
#include "event_detection.hpp"
#include "event_handler.hpp"
#include "event_info.hpp"
#include "log.hpp"
#include "message_composer.hpp"
#include "selftest.hpp"

#include <boost/container/flat_map.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <string>
#include <vector>

using namespace std;
using namespace phosphor::logging;

const auto APPNAME = "oobamld";
const auto APPVER = "0.1";

log_init;

namespace aml
{
namespace profile
{

event_info::EventMap eventMap;
std::map<std::string, dat_traverse::Device> datMap;

} // namespace profile

struct Configuration
{
    std::string dat;
    std::string event;
};

Configuration configuration;

int loadDAT(cmd_line::ArgFuncParamType params)
{
    if (params[0].size() == 0)
    {
        cout << "Need a parameter!"
             << "\n";
        return -1;
    }

    fstream f(params[0]);
    if (!f.is_open())
    {
        throw std::runtime_error("File (" + params[0] + ") not found!");
    }

    configuration.dat = params[0];

    // dat_traverse::Device::printTree(profile::datMap);

    return 0;
}

int loadEvents(cmd_line::ArgFuncParamType params)
{
    if (params[0].size() == 0)
    {
        cout << "Need a parameter!"
             << "\n";
        return -1;
    }

    fstream f(params[0]);
    if (!f.is_open())
    {
        throw std::runtime_error("File (" + params[0] + ") not found!");
    }

    configuration.event = params[0];

    return 0;
}

int show_help([[maybe_unused]] cmd_line::ArgFuncParamType params);

cmd_line::CmdLineArgs cmdLineArgs = {
    {"-h", "--help", cmd_line::OptFlag::none, "", cmd_line::ActFlag::exclusive,
     "This help.", show_help},
    {"-d", "", cmd_line::OptFlag::overwrite, "<file>",
     cmd_line::ActFlag::mandatory, "Device Association Tree filename.",
     loadDAT},
    {"-e", "", cmd_line::OptFlag::overwrite, "<file>",
     cmd_line::ActFlag::normal, "Event Info List filename.", loadEvents},
};

int show_help([[maybe_unused]] cmd_line::ArgFuncParamType params)
{
    cout << "NVIDIA Active Monitoring & Logging Service, ver = " << APPVER
         << "\n";
    cout << "<usage>\n";
    cout << "  ./" << APPNAME << " [options]\n";
    cout << "\n";
    cout << "options:\n";
    cout << cmd_line::CmdLine::showHelp(cmdLineArgs);
    cout << "\n";

    return 0;
}

sd_bus* bus = nullptr;

} // namespace aml

////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Service Entry Point
 */
int main(int argc, char* argv[])
{
    int rc = 0;
#if 0
    message_composer::MessageComposer mc("MsgComp1");
    event_info::EventNode ev("OverT");
    ev.device = "GPU0";
    ev.messageRegistry.messageId = "ResourceEvent.1.0.ResourceErrorsDetected";
    ev.messageRegistry.message.severity = "Critical";
    ev.messageRegistry.message.resolution = "ask me";
    mc.process(ev);

    return 0;
#endif
    try
    {
        cmd_line::CmdLine cmdLine(argc, argv, aml::cmdLineArgs);
        rc = cmdLine.parse();
        rc = cmdLine.process();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[E]" << e.what() << "\n";
        aml::show_help({});
        return rc;
    }

    std::cerr << "Trying to load Events from file\n";

    // Initialization
    event_info::loadFromFile(aml::profile::eventMap, aml::configuration.event);

    // event_info::printMap(aml::profile::eventMap);

    dat_traverse::Device::populateMap(aml::profile::datMap,
                                      aml::configuration.dat);

    // Register event handlers
    message_composer::MessageComposer msgComposer("MsgComp1");
    event_handler::DATTraverse datTraverser("DatTraverser1");
    datTraverser.setDAT(aml::profile::datMap);

    event_handler::ClearEvent clearEvent;
    event_handler::EventHandlerManager eventHdlrMgr("EventHandlerManager");

    selftest::Selftest selfTester("SelfTester1", aml::profile::datMap);

    eventHdlrMgr.RegisterHandler(&datTraverser);
    eventHdlrMgr.RegisterHandler(&msgComposer);
    eventHdlrMgr.RegisterHandler(&clearEvent);

    cout << "Creating " << oob_aml::SERVICE_BUSNAME << "\n";

    rc = sd_bus_default_system(&aml::bus);
    if (rc < 0)
    {
        cout << "Failed to connect to system bus"
             << "\n";
    }

    auto io = std::make_shared<boost::asio::io_context>();
    auto sdbusp = std::make_shared<sdbusplus::asio::connection>(*io, aml::bus);

    sdbusp->request_name(oob_aml::SERVICE_BUSNAME);
    auto server = sdbusplus::asio::object_server(sdbusp);
    auto iface =
        server.add_interface(oob_aml::TOP_OBJPATH, oob_aml::SERVICE_IFCNAME);

    event_detection::EventDetection eventDetection(
        "EventDetection1", &aml::profile::eventMap, &eventHdlrMgr);
    auto eventMatcher = event_detection::EventDetection::startEventDetection(
        &eventDetection, sdbusp);

    iface->initialize();

    try
    {}
    catch (const std::exception& e)
    {
        std::cerr << "[E]" << e.what() << "\n";
        return rc;
    }

    io->run();

    return 0;
}
