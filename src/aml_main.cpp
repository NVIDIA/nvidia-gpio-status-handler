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
#include "cmd_line.hpp"
#include "dat_traverse.hpp"
#include "event_info.hpp"
#include <boost/container/flat_map.hpp>
#include <fstream>
#include <iostream>
#include <map>
#include <phosphor-logging/log.hpp>
#include <queue>
#include <sdbusplus/asio/object_server.hpp>
#include "cmd_line.hpp"
#include "aml_main.hpp"
#include "event_detection.hpp"
#include <string>
#include <vector>

using namespace std;
using namespace phosphor::logging;

const auto APPNAME = "oobamld";
const auto APPVER = "0.1";

namespace aml {
namespace profile {

std::map<std::string, std::vector<event_info::EventNode>> eventMap;
std::map<std::string, dat_traverse::Device> datMap;

} // namespace profile

struct Configuration {
  std::string dat;
  std::string event;
};

Configuration configuration;

int loadDAT(cmd_line::ArgFuncParamType params) {
  if (params[0].size() == 0) {
    cout << "Need a parameter!"
         << "\n";
    return -1;
  }

  fstream f(params[0]);
  if (!f.is_open()) {
    throw std::runtime_error("File (" + params[0] + ") not found!");
  }

  configuration.dat = params[0];

  std::ifstream i(configuration.dat);
  json j;
  i >> j;

  dat_traverse::Device::populateMap(profile::datMap, j);

  //dat_traverse::Device::printTree(profile::datMap);

  return 0;
}

int loadEvents(cmd_line::ArgFuncParamType params) {
  if (params[0].size() == 0) {
    cout << "Need a parameter!"
         << "\n";
    return -1;
  }

  fstream f(params[0]);
  if (!f.is_open()) {
    throw std::runtime_error("File (" + params[0] + ") not found!");
  }

  configuration.event = params[0];

  std::ifstream i(configuration.event);
  json j;
  i >> j;

  event_info::EventNode::populateMap(profile::eventMap, j);

  //event_info::EventNode::printMap(profile::eventMap);

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

int show_help([[maybe_unused]] cmd_line::ArgFuncParamType params) {
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

sd_bus *bus = nullptr;

} // namespace aml



////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Service Entry Point
 */
int main(int argc, char* argv[])
{
    int rc = 0;
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

    cout << "Creating " << oob_aml::SERVICE_BUSNAME << "\n";

    rc = sd_bus_default_system(&aml::bus);
    if (rc < 0)
    {
        cout << "Failed to connect to system bus"
             << "\n";
    }

    auto io = std::make_shared<boost::asio::io_context>();
    auto sdbusp =
        std::make_shared<sdbusplus::asio::connection>(*io, aml::bus);

    sdbusp->request_name(oob_aml::SERVICE_BUSNAME);
    auto server = sdbusplus::asio::object_server(sdbusp);
    auto iface = server.add_interface(oob_aml::TOP_OBJPATH, oob_aml::SERVICE_IFCNAME);
    

    auto eventDetection = event_detection::EventDetection::startEventDetection(sdbusp, iface);

    iface->initialize();
        
    try
    {
    }
    catch (const std::exception& e)
    {
        std::cerr << "[E]" << e.what() << "\n";
        return rc;
    }

    io->run();

    return 0;
}
