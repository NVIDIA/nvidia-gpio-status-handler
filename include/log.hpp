/**
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#pragma once

/**
 * SIMLOG - A SIMple LOG
 *
 * as simple as just do,
 *   #include "log.hpp"
 * before using its APIs.
 *
 * It supports to set debug log level in both compiling time or runtime.
 **/

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <cstdarg>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <vector>

namespace logging
{

/**
 * Set debug level controller by,
 * #define DBG_LOG_CTRL x
 * #include "log.hpp"
 */
#ifndef DBG_LOG_CTRL
#define DBG_LOG_CTRL "nvgpuoob_ctrl"
#endif

/**
 * Debug Level Definition
 * 0 : Log disabled
 * 1 : Error log only (default)
 * 2 : Error & Warning logs
 * 3 : Error & Warning & Debug logs
 * 4 : Error & Warning & Debug & Info logs
 */
enum LogLevel
{
    disabled = 0,
    error = 1,
    warning = 2,
    debug = 3,
    information = 4,

    dataonly = 0x8000 | information,
};

/**
 * Set default debug level by,
 * #define DEF_DBG_LEVEL x
 * #include "log.hpp"
 */
#ifndef DEF_DBG_LEVEL
#define DEF_DBG_LEVEL disabled
#endif

class Log
{
    using CtrlType = int;
#define getLogLevel(logFlag) ((logFlag)&0x00FF)
#define getLogControl(logFlag) ((logFlag)&0xFF00)
  public:
    Log(const std::string& file = "", CtrlType level = DEF_DBG_LEVEL) :
        logFile(file), initLevel(level), logCtrlName(DBG_LOG_CTRL), seq(0)
    {
        openLogFile();
        smInit();
    }

    ~Log()
    {
        smDeinit();
        closeLogFile();
    }

    void setLevel(CtrlType desiredLevel = DEF_DBG_LEVEL)
    {
        std::lock_guard<std::mutex> logGuard(lMutex);
        setCtrlLevel(desiredLevel);
    }

    CtrlType getLevel(void)
    {
        return getCtrlLevel();
    }

    void setLogFile(const std::string& file)
    {
        closeLogFile();
        logFile = file;
        openLogFile();
    }

    void log(int desiredLevel, const char* fmt, ...)
    {
        if (!isReady)
        {
            return;
        }
        if (getLogLevel(getLevel()) >= getLogLevel(desiredLevel))
        {
            std::stringstream ss;
            if (!(getLogControl(desiredLevel) & LogLevel::dataonly))
            {
                // Timestamp
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                char buf[100] = {0};
                strftime(buf, sizeof(buf), "%D %T", gmtime(&ts.tv_sec));

                // ss << seq++; // Debug purpose only to check if any log
                // missing

                ss << "[" << buf << "." << std::setfill('0') << std::setw(9)
                   << ts.tv_nsec << "]";

                // Severity
                switch (getLogLevel(desiredLevel))
                {
                    case LogLevel::error:
                        ss << "E";
                        break;
                    case LogLevel::warning:
                        ss << "W";
                        break;
                    case LogLevel::debug:
                        ss << "D";
                        break;
                    case LogLevel::information:
                        ss << "I";
                        break;
                    default:
                        ss << "O";
                        break;
                }
            }

            // Message
            char msg[1024] = {0};
            va_list args;
            va_start(args, fmt);
            vsnprintf(msg, sizeof(msg), fmt, args);
            va_end(args);

            ss << msg;

            outputLog(ss.str());
        }
    }

    void log_raw(int desiredLevel, const char* msg,
                 const std::vector<uint8_t>& array, size_t size)
    {
        if (!isReady)
        {
            return;
        }
        if (getLogLevel(getLevel()) >= getLogLevel(desiredLevel))
        {
            std::stringstream ss;
            // Timestamp
            ss << timestampString();

            // Severity
            ss << "I";

            ss << "[raw]:";

            // Prompt
            ss << msg;

            // Raw data
            ss << "(" << size << ") ";
            for (int i = 0; i < size; i++)
            {
                ss << std::setfill('0') << std::setw(2) << std::hex
                   << int(array[i]) << " ";
            }
            ss << "\n";

            outputLog(ss.str());
        }
    }

    void log_raw(int desiredLevel, const char* msg,
                 const std::vector<uint32_t>& array, size_t size)
    {
        if (!isReady)
        {
            return;
        }
        if (getLogLevel(getLevel()) >= getLogLevel(desiredLevel))
        {
            std::stringstream ss;
            // Timestamp
            ss << timestampString();

            // Severity
            ss << "I";

            ss << "[raw]:";

            // Prompt
            ss << msg;

            // Raw data
            ss << "(" << size << ") ";
            for (int i = 0; i < size; i++)
            {
                ss << std::setfill('0') << std::setw(8) << std::hex
                   << int(array[i]) << " ";
            }
            ss << "\n";

            outputLog(ss.str());
        }
    }

    void log_raw(int desiredLevel, const char* msg, const uint8_t* array,
                 size_t size)
    {
        std::vector<uint8_t> arr(array, array + size);
        log_raw(desiredLevel, msg, arr, arr.size());
    }

    void log_raw(int desiredLevel, const char* msg, const uint32_t* array,
                 size_t size)
    {
        std::vector<uint32_t> arr(array, array + size);
        log_raw(desiredLevel, msg, arr, arr.size());
    }

  private:
    std::mutex lMutex;

    std::string logFile;
    std::string logCtrlName;
    std::fstream logStream;

    int smfd;
    CtrlType* ctrlLevel;

    CtrlType initLevel;

    bool isReady;

    unsigned long long seq;

    CtrlType getCtrlLevel() const
    {
        return *ctrlLevel - '0';
    }
    void setCtrlLevel(const CtrlType& level)
    {
        *ctrlLevel = level + '0';
    }

    void openLogFile()
    {
        if (logFile.size() == 0)
        {
            return;
        }

        // to file
        logStream.open(logFile, std::ios::out);
        if (!logStream.is_open())
        {
            throw std::runtime_error("Log file (" + logFile + ") open failed!");
        }
    }

    void closeLogFile()
    {
        // to file
        if (logStream.is_open())
        {
            log(LogLevel::information, "=========== End ===========\n");
            logStream.flush();
            logStream.close();
        }
    }

    void outputLog(const std::string& msg)
    {
        std::lock_guard<std::mutex> logGuard(lMutex);
        if (logStream.is_open())
        {
            logStream << msg;
        }
        else
        {
            // don't allow to print to console
            // std::cout << msg;
        }
    }

    const std::string timestampString()
    {
        std::stringstream ss;
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        char buf[100] = {0};
        strftime(buf, sizeof(buf), "%D %T", gmtime(&ts.tv_sec));

        // ss << seq++; // Debug purpose only to check if any log missing

        ss << "[" << buf << "." << std::setfill('0') << std::setw(9)
           << ts.tv_nsec << "]";

        return ss.str();
    }

    int smInit()
    {
        bool isFirstSMHdl = true;
        smfd = shm_open(logCtrlName.c_str(), O_EXCL | O_CREAT | O_RDWR,
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        auto er = errno;
        if (-1 == smfd)
        {
            if (EEXIST == er) // Already exists
            {
                smfd = shm_open(logCtrlName.c_str(), O_RDWR,
                                S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
                if (-1 == smfd)
                {
                    throw std::runtime_error("Log Ctrl open failed (" +
                                             std::string(strerror(er)) + ")!");
                }
                isFirstSMHdl = false;
            }
            else
            {
                throw std::runtime_error("Log Ctrl init failed (" +
                                         std::string(strerror(er)) + ")!");
            }
        }

        int rc = ftruncate(smfd, sizeof(CtrlType));
        if (-1 == rc)
        {
            throw std::runtime_error("SMEM truncate failed!");
        }

        ctrlLevel =
            (CtrlType*)mmap(NULL, sizeof(CtrlType), PROT_READ | PROT_WRITE,
                            MAP_SHARED, smfd, 0);
        if (ctrlLevel == MAP_FAILED)
        {
            throw std::runtime_error("Map failed!");
        }

        if (isFirstSMHdl)
        {
            setLevel(initLevel);
        }

        isReady = true;

        return 0;
    }

    int smDeinit()
    {
        int rc = munmap(ctrlLevel, sizeof(*ctrlLevel));
        if (-1 == rc)
        {
            throw std::runtime_error("Unmap failed!");
        }

        isReady = false;

        rc = close(smfd);
        if (-1 == rc)
        {
            throw std::runtime_error ("close(smfd) failed!");
        }

        return 0;
    }
};

} // namespace logging

using namespace logging;
extern Log logger;

/**
 * log_init; is necessarily to be called before using any log function in
 *  a process.
 **/
#define log_init Log logger

/**
 * log_set_file() for changing output
 **/
#define log_set_file(file) logger.setLogFile(file)

/**
 * log_set_level() is to change debug log level
 **/
#define log_set_level(dl) logger.setLevel(dl)

/**
 * log_get_level() is to get debug log level
 **/
#define log_get_level() logger.getLevel()

/**
 * Use any following log functions for debug logs.
 * log_* for using in class non-static member functions
 * logs_* for using in static and global functions
 **/
#define log_err(fmt, ...)                                                      \
    logger.log(LogLevel::error, "[%s][%s]: " fmt, typeid(*this).name(),        \
               __func__, ##__VA_ARGS__)
#define log_wrn(fmt, ...)                                                      \
    logger.log(LogLevel::warning, "[%s][%s]: " fmt, typeid(*this).name(),      \
               __func__, ##__VA_ARGS__)
#define log_dbg(fmt, ...)                                                      \
    logger.log(LogLevel::debug, "[%s][%s]: " fmt, typeid(*this).name(),        \
               __func__, ##__VA_ARGS__)
#define log_info(fmt, ...)                                                     \
    logger.log(LogLevel::information, "[%s][%s]: " fmt, typeid(*this).name(),  \
               __func__, ##__VA_ARGS__)
#define log_info_raw(fmt, array, size)                                         \
    logger.log_raw(LogLevel::information, fmt, array, size)

#define logs_err(fmt, ...)                                                     \
    logger.log(LogLevel::error, "[%s:%d][%s]: " fmt, __FILE__, __LINE__,       \
               __func__, ##__VA_ARGS__)
#define logs_wrn(fmt, ...)                                                     \
    logger.log(LogLevel::warning, "[%s:%d][%s]: " fmt, __FILE__, __LINE__,     \
               __func__, ##__VA_ARGS__)
#define logs_dbg(fmt, ...)                                                     \
    logger.log(LogLevel::debug, "[%s:%d][%s]: " fmt, __FILE__, __LINE__,       \
               __func__, ##__VA_ARGS__)
#define logs_info(fmt, ...)                                                    \
    logger.log(LogLevel::information, "[%s:%d][%s]: " fmt, __FILE__, __LINE__, \
               __func__, ##__VA_ARGS__)
#define logs_info_raw(fmt, array, size) log_info_raw(fmt, array, size)
