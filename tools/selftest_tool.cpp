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
#include "dat_traverse.hpp"
#include "log.hpp"
#include "selftest.hpp"

#include <phosphor-logging/log.hpp>

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

/* TODO: multiple log_init causes issues in prog termination */
// log_init;

const std::string default_report_name = "selftest_report.json";

struct Configuration
{
    /** @brief DAT file path **/
    std::string dat;
    /** @brief device name to test or entire tree if empty **/
    std::string targetDevice;
    /** @brief output report file path **/
    std::string report;
};

Configuration configuration = {
    .dat = "", .targetDevice = "", .report = default_report_name};

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
     cmd_line::ActFlag::normal,
     "Target Device for testing. Default: whole DAT tree", loadTargetDevice},
    {"-r", "", cmd_line::OptFlag::overwrite, "<file>",
     cmd_line::ActFlag::normal,
     "Report file path. Default: " + default_report_name, loadReportPath},
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
        cmd_line::CmdLine cmdLine(argc, argv, cmdLineArgs);
        rc = cmdLine.parse();
        rc = cmdLine.process();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[E]" << e.what() << "\n";
        show_help({});
        return rc;
    }

    // std::cout << "DAT: " << configuration.dat << "\n";
    // std::cout << "Target: " << configuration.targetDevice << "\n";
    // std::cout << "Report path: " << configuration.report << "\n";

    std::map<std::string, dat_traverse::Device> datMap;
    dat_traverse::Device::populateMap(datMap, configuration.dat);

    if (configuration.targetDevice.size())
    {
        if (datMap.find(configuration.targetDevice) == datMap.end())
        {
            throw std::runtime_error("Device [" + configuration.targetDevice +
                                     "] not found in provided DAT file!");
        }
    }

    selftest::Selftest selftest("selfTest", datMap);
    selftest::ReportResult reportResult;
    aml::RcCode result = aml::RcCode::succ;

    try
    {
        if (configuration.targetDevice.size())
        {
            auto& dev = datMap.at(configuration.targetDevice);
            result = selftest.perform(dev, reportResult);
        }
        else
        {
            result = selftest.performEntireTree(reportResult);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }

    if (result != aml::RcCode::succ)
    {
        return -1;
    }

    selftest::Report reportGenerator;
    if (!reportGenerator.generateReport(reportResult))
    {
        std::cerr << "Error: failed to generate report!" << std::endl;
        return -1;
    }

    std::ofstream ofile(configuration.report);
    ofile << reportGenerator;

    return 0;
}
