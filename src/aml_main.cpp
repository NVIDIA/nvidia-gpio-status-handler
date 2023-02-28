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
#include "cmd_line.hpp"
#include "dat_traverse.hpp"
#include "event_detection.hpp"
#include "event_info.hpp"
#include "message_composer.hpp"
#include "pc_event.hpp"
#include "selftest.hpp"
#include "threadpool_manager.hpp"

#include <unistd.h>

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

/**
 * @brief RETRY_DBUS_INFO_COUNTER and RETRY_SLEEP are used to wait for other
 *        services populate their data on DBus
 */
constexpr int RETRY_DBUS_INFO_COUNTER = 60;
constexpr int RETRY_SLEEP = 5;

#ifndef DEFAULT_RUNNING_THREAD_LIMIT
#define DEFAULT_RUNNING_THREAD_LIMIT 3
#endif

// DEFAULT_TOTAL_THREAD_LIMIT must be greater than
// DEFAULT_RUNNING_THREAD_LIMIT if you want to allow blocking until
// a slot is free. If they are equal, thread creation at the limit
// will immediately fail.
#ifndef DEFAULT_TOTAL_THREAD_LIMIT
#define DEFAULT_TOTAL_THREAD_LIMIT 4
#endif

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
    int running_thread_limit = DEFAULT_RUNNING_THREAD_LIMIT;
    int total_thread_limit = DEFAULT_TOTAL_THREAD_LIMIT;
};

Configuration configuration;

int loadDAT(cmd_line::ArgFuncParamType params)
{
    if (params[0].size() == 0)
    {
        logs_dbg("Need a parameter!\n");
        return -1;
    }

    ifstream f(params[0]);
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
        logs_dbg("Need a parameter!\n");
        return -1;
    }

    ifstream f(params[0]);
    if (!f.is_open())
    {
        throw std::runtime_error("File (" + params[0] + ") not found!");
    }

    configuration.event = params[0];

    return 0;
}

int setLogLevel(cmd_line::ArgFuncParamType params)
{
    int newLvl = std::stoi(params[0]);

    if (newLvl < 0 || newLvl > 5)
    {
        throw std::runtime_error("Level of our range[0-4]!");
    }

    log_set_level(newLvl);

    return 0;
}

int setLogFile(cmd_line::ArgFuncParamType params)
{
    if (!params[0].size())
    {
        throw std::runtime_error("Need a file name!");
    }

    log_set_file(params[0].c_str());

    return 0;
}

int setDbusDelay(cmd_line::ArgFuncParamType params)
{
    int delay = std::stoi(params[0]);
    if (delay < 0)
    {
        throw std::runtime_error("Dbus delay cannot be lesser than 0");
    }
    dbus::defaultDbusDelayer.setDelayTime(std::chrono::milliseconds(delay));
    return 0;
}

int setRunningThreadLimit(cmd_line::ArgFuncParamType params)
{
    int threads = std::stoi(params[0]);
    if (threads <= 0)
    {
        throw std::runtime_error("Event thread count cannot be less than 1");
    }
    configuration.running_thread_limit = threads;
    return 0;
}

int setTotalThreadLimit(cmd_line::ArgFuncParamType params)
{
    int threads = std::stoi(params[0]);
    if (threads <= 0)
    {
        throw std::runtime_error("Event thread count cannot be less than 1");
    }
    configuration.total_thread_limit = threads;
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
    {"-l", "", cmd_line::OptFlag::overwrite, "<level>",
     cmd_line::ActFlag::normal, "Debug Log Level [0-4].", setLogLevel},
    {"-L", "", cmd_line::OptFlag::overwrite, "<file>",
     cmd_line::ActFlag::normal, "Debug Log file. Use stdout if omitted.",
     setLogFile},
    {"-s", "--dbus-space", cmd_line::OptFlag::overwrite, "<file>",
     cmd_line::ActFlag::normal,
     "Minimal amount of time (in ms) between dbus calls"
     " (from the finish of the last one to the start of the current)",
     setDbusDelay},
    {"-t", "--running-threads", cmd_line::OptFlag::overwrite, "<num>",
     cmd_line::ActFlag::normal,
     "Maximum number of simultaneous running event handling threads"
     " (from the finish of the last one to the start of the current)",
     setRunningThreadLimit},
    {"-T", "--total-threads", cmd_line::OptFlag::overwrite, "<num>",
     cmd_line::ActFlag::normal,
     "Maximum number of simultaneous running + queued event handling threads"
     " (from the finish of the last one to the start of the current)",
     setTotalThreadLimit}};

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

//sd_bus* bus = nullptr;

} // namespace aml

void startWorkerThread(std::shared_ptr<boost::asio::io_context> io)
{
    auto thread = std::make_unique<std::thread>([io]() {
        logs_err("Creating worker thread\n");
        event_detection::EventDetection::workerThreadMainLoop();
        // the main loop exited for whatever reason, so
        // queue a task to the main thread to restart the worker thread
        logs_err("worker thread event loop exited unexpectedly, restarting it\n");
        io->post([io]() {
            startWorkerThread(io);
        });
    });
    thread->detach();
}

////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Service Entry Point
 */
int main(int argc, char* argv[])
{
    int rc = 0;
#if 0
    message_composer::MessageComposer mc(aml::profile::datMap, "MsgComp1");
    event_info::EventNode ev("OverT");
    ev.device = "GPU0";
    ev.messageRegistry.messageId = "ResourceEvent.1.0.ResourceErrorsDetected";
    ev.messageRegistry.message.severity = "Critical";
    ev.messageRegistry.message.resolution = "ask me";
    mc.process(ev);

    return 0;
#endif
    logger.setLevel(DEF_DBG_LEVEL);
    logs_info("Default log level: %d. Current log level: %d\n", DEF_DBG_LEVEL,
              getLogLevel(logger.getLevel()));
    try
    {
        cmd_line::CmdLine cmdLine(argc, argv, aml::cmdLineArgs);
        rc = cmdLine.parse();
        rc = cmdLine.process();
    }
    catch (const std::exception& e)
    {
        logs_err("%s\n", e.what());
        aml::show_help({});
        return rc;
    }
    logs_err("Trying to load Events from file\n");

    // Initialization
    event_info::loadFromFile(aml::profile::eventMap, aml::configuration.event);

    // event_info::printMap(aml::profile::eventMap);

    dat_traverse::Device::populateMap(aml::profile::datMap,
                                      aml::configuration.dat);

    // Register event handlers
    message_composer::MessageComposer msgComposer(aml::profile::datMap,
                                                  "MsgComp1");
    event_handler::DATTraverse datTraverser("DatTraverser1");
    datTraverser.setDAT(aml::profile::datMap);

    int retryGettingDbusInfo = 0;
    bool error = true;
    while (error == true && retryGettingDbusInfo < RETRY_DBUS_INFO_COUNTER)
    {
        try
        {
            datTraverser.datToDbusAssociation();
            error = false; // stop the loop
        }
        catch (const std::exception& e)
        {
            if (++retryGettingDbusInfo < RETRY_DBUS_INFO_COUNTER)
            {
                logs_wrn("waiting Dbus information ...\n");

                ::sleep(RETRY_SLEEP);
                continue;
            }
            logs_err(
                "HealthRollup & OriginOfCondition can't be supported at the moment due to Dbus Error.\n");
        }
    }

    // Create threadpool manager
    event_detection::threadpoolManager = std::make_unique<ThreadpoolManager>(
        aml::configuration.running_thread_limit,
        aml::configuration.total_thread_limit);

    event_detection::queue = std::make_unique<PcQueueType>(PROPERTIESCHANGED_QUEUE_SIZE);

    event_handler::ClearEvent clearEvent("ClearEvent");
    event_handler::EventHandlerManager eventHdlrMgr("EventHandlerManager");
    event_handler::RootCauseTracer rootCauseTracer("RootCauseTracer",
                                                   aml::profile::datMap);

    selftest::Selftest selftest("bootupSelftest", aml::profile::datMap);
    selftest::ReportResult rep_res;
 
    event_detection::EventDetection eventDetection(
        "EventDetection1", &aml::profile::eventMap, &eventHdlrMgr);

    auto thread = std::make_unique<
        std::thread>([rep_res, selftest, eventDetection]() mutable {
        logs_wrn("started bootup selftest\n");
        ThreadpoolGuard guard(event_detection::threadpoolManager.get());
        if (!guard.was_successful())
        {
            // the threadpool has reached the max queued tasks limit,
            // don't run this event thread
            logs_err(
                "Thread pool over maxTotal tasks limit, exiting event thread\n");
            return;
        }
        if (selftest.performEntireTree(rep_res,
                                       std::vector<std::string>{"data_dump"}) !=
            aml::RcCode::succ)
        {
            logs_err("Bootup Selftest failed\n");
            return;
        }
        for (const auto& entry : rep_res)
        {
            logs_dbg("SelfTest Device: %s\n", entry.first.c_str());
            if (selftest.evaluateDevice(entry.second))
            {
                logs_dbg(
                    "Device %s healthy based on SelfTest. Performing recovery actions.\n",
                    entry.first.c_str());
                eventDetection.updateDeviceHealthAndResolve(
                    entry.first, std::string(""));
            }
            else
            {
                logs_err("SelfTest for Device %s failed\n", entry.first.c_str());
            }
        }
        logs_wrn("finished bootup selftest\n");
    });

    if (thread != nullptr)
    {
        thread->detach();
    }
    else
    {
        logs_err("Create thread to process event failed!\n");
    }

    /* Event handlers registration order is important - msgComposer uses data
    acquired by previous handlers; handlers are used in registration order. */
    eventHdlrMgr.RegisterHandler(&datTraverser);
    eventHdlrMgr.RegisterHandler(&rootCauseTracer);
    eventHdlrMgr.RegisterHandler(&msgComposer);
    eventHdlrMgr.RegisterHandler(&clearEvent);

    logs_dbg("Creating %s\n", (const char*)oob_aml::SERVICE_BUSNAME);
    sd_bus* mainThreadBus = nullptr;
    rc = sd_bus_default_system(&mainThreadBus);
    logs_dbg("main thread dbus connection is %p\n", mainThreadBus);
    if (rc < 0)
    {
        logs_dbg("Failed to connect to system bus\n");
    }

    try
    {
        auto io = std::make_shared<boost::asio::io_context>();

        startWorkerThread(io);

        auto sdbusp =
            std::make_shared<sdbusplus::asio::connection>(*io, mainThreadBus);

        sdbusp->request_name(oob_aml::SERVICE_BUSNAME);
        auto server = sdbusplus::asio::object_server(sdbusp);
        auto iface = server.add_interface(oob_aml::TOP_OBJPATH,
                                          oob_aml::SERVICE_IFCNAME);
        auto eventMatcher =
            eventDetection.startEventDetection(&eventDetection, sdbusp);

        iface->initialize();

        logs_err("NVIDIA OOB AML daemon is ready.\n");
        io->run();
    }
    catch (const std::exception& e)
    {
        logs_err("%s\n", e.what());
        return -1;
    }

    return 0;
}
