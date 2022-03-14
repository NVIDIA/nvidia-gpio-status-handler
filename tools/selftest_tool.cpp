/**
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "aml.hpp"
#include "cmd_line.hpp"
#include "log.hpp"
#include "selftest.hpp"

#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <string>
#include <vector>

using namespace std;
using namespace phosphor::logging;

const auto APPNAME = "selftest_tool";
const auto APPVER = "0.1";

log_init;

struct Configuration
{
    std::string dat;
    std::string targetDevice;
    std::string report;
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

    return 0;
}

int loadTargetDevice(cmd_line::ArgFuncParamType params)
{
    if (params[0].size() == 0)
    {
        cout << "Need a parameter!"
             << "\n";
        return -1;
    }

    configuration.targetDevice = params[0];

    return 0;
}

int loadReportPath(cmd_line::ArgFuncParamType params)
{
    if (params[0].size() == 0)
    {
        cout << "Need a parameter!"
             << "\n";
        return -1;
    }

    configuration.report = params[0];

    return 0;
}

int show_help([[maybe_unused]] cmd_line::ArgFuncParamType params);

cmd_line::CmdLineArgs cmdLineArgs = {
    {"-h", "--help", cmd_line::OptFlag::none, "", cmd_line::ActFlag::exclusive,
     "This help.", show_help},
    {"-d", "", cmd_line::OptFlag::overwrite, "<file>",
     cmd_line::ActFlag::mandatory, "Device Association Tree filename.",
     loadDAT},
    {"-D", "", cmd_line::OptFlag::overwrite, "<device>",
     cmd_line::ActFlag::mandatory, "Target Device for testing",
     loadTargetDevice},
    {"-r", "", cmd_line::OptFlag::overwrite, "<file>",
     cmd_line::ActFlag::normal, "Report file path.", loadReportPath},
};

int show_help([[maybe_unused]] cmd_line::ArgFuncParamType params)
{
    cout << "NVIDIA Selftest Tool, ver = " << APPVER << "\n";
    cout << "<usage>\n";
    cout << "  ./" << APPNAME << " [options]\n";
    cout << "\n";
    cout << "options:\n";
    cout << cmd_line::CmdLine::showHelp(cmdLineArgs);
    cout << "\n";

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Entry Point
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

    // SelfTest Logic

    return 0;
}
