
#include <gpio_chips.hpp>
#include <gpio_json_config.hpp>
#include <gpio_lines.hpp>
#include <gpio_status_handler.hpp>
#include <gpio_utils.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/server.hpp>

#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

using namespace std;
using json = nlohmann::json;
using namespace gpio_handler;

using phosphor::logging::entry;
using phosphor::logging::level;
using phosphor::logging::log;

constexpr auto dbusObjectPath = "/xyz/openbmc_project/GpioStatusHandler";
constexpr auto dbusServiceName = "xyz.openbmc_project.GpioStatusHandler";
constexpr auto dbusInterfaceName = "xyz.openbmc_project.GpioStatus";

/**
 * @brief Second argument to @gpiod_line_event_wait function
 *
 * Specifies also the maximal resolution of the
 * @GpioJsonConfig::configKeyReadPeriod parameter given in the config file.
 */
constexpr uint64_t lineEventWaitTimeoutNs = 1e8l; // [nanoseconds]

static boost::asio::io_context io;
static volatile bool runThreads;
static std::mutex setDBusPropMutex;
static exception_ptr lastThreadException;
static int threadsExitCode;

void stopService(int exitCode)
{
#ifdef ENABLE_GSH_LOGS
    log<level::INFO>("Stopping service");
#endif
    threadsExitCode = exitCode;
    runThreads = false;
    io.stop();
}

bool setDBusProperty(shared_ptr<sdbusplus::asio::dbus_interface> dbusInterface,
                     const string& pinName, const string& chipName, int pinNum,
                     bool pinValue) noexcept
{
    bool success = false;
    {
        lock_guard<mutex> lock(setDBusPropMutex);
#ifdef ENABLE_GSH_LOGS
        /* TODO: this log message is notice only - it should not be called
           always, but only if verbosity level is set to level notice at least.
            Additionally, it should be called only if the pin value is changed.
         */
        {
            stringstream ss;
            ss << "Setting '" << dbusInterfaceName << "." << pinName << "' <- "
               << pinValue;
            logPinOperation<level::INFO>(ss.str().c_str(), pinName, chipName,
                                         pinNum);
        }
#endif
        try
        {
            dbusInterface->set_property(pinName, pinValue);
            success = true;
        }
        catch (...) // Catch most possible number of exceptions
        {
            lastThreadException = std::current_exception();
            success = false;
        }
    }
    if (!success)
    {
        stringstream ss;
        ss << "Unable to set the property '" << dbusInterface << "." << pinName
           << "' to " << pinValue;
        logPinOperation<level::ERR>(ss.str().c_str(), pinName, chipName,
                                    pinNum);
    }
    return success;
}

/**
 * @brief Entry function for the gpio monitoring threads
 *
 * The dbus property update scheme is a mix of polling and event detection. So
 * basically the change of the pin should result in an immediate change of the
 * dbus property, but there is also periodic polling "just in case", specified
 * per gpio line by the "read_period_sec", setting the lower bound for gpio
 * refresh rate.
 *
 * To understand it better consider the following graph:
 *
 *
 * Timeline
 * GPIO line:
 *                                  state change
 * 1                                      --------------------------------------
 *                                       /
 * 0 ------------------------------------
 *
 * Loop iteratons                              timeout period
 * (no longer than timeout):                      <----->
 *   |-------|-------|-------|------|----|-------|-------|-------|------|-------
 *
 * Readings:
 *   ^------------------------------^----^------------------------------^-------
 *   0                              0    1                              1
 *    <---------------------------->      <---------------------------->
 *          polling period                        polling period
 *
 * DBus property:                   event detected
 * t                                      --------------------------------------
 *                                       /
 * f ------------------------------------
 *
 *
 * The correspondence to the parameters is as follows:
 * "GPIO line" : specified by parameters @chip and @line.
 * "state change" : detected in 'gpiod_line_event_wait' call inside this
 * function.
 * "timeout period" = @lineEventWaitTimeoutNs
 * "polling period" = @readPeriodTicks
 * "DBus property" : the @pinName property on the DBus interface @dbusInterface
 *
 * The function can stop execution at discrete points spaced by "timeot period"
 * or gpio pin state change (and only then) in case: 1. global @runThreads was
 * set to false by different thread, 2. error occured when calling any of the
 * functions 'gpiod_line_event_wait' or 'gpiod_line_get_value' from the gpiod
 * library, 3. an error occured when calling 'set_property' function on the
 * @dbusInterface object. Otherwise the function continue to run. No exceptions
 * are ever thrown.
 *
 * @param[in] dbusInterface The DBus interface for which the @pinName boolean
 * property is expected to exist and will be updated according to the state of
 * the monitored gpio pin.
 * @param[in] line
 * @param[in] chipName
 * @param[in] pinName
 * @param[in] pinNum
 * @param[in] readPeriodTicks
 */
void syncAlertGpioPin(
    shared_ptr<sdbusplus::asio::dbus_interface> dbusInterface,
    gpiod_line_t* line, const string& chipName, const string& pinName,
    unsigned pinNum,
    uint64_t readPeriodTicks // [nanoseconds / lineEventWaitTimeoutNs]
)
{
    struct timespec timeout
    {
        lineEventWaitTimeoutNs / 1000000000, lineEventWaitTimeoutNs % 1000000000
    };

    struct gpiod_line_event event;

    uint64_t ticks = 0;
    bool setDBusPropOk = true;
    int lineGetResult = 0;
    // -1: error
    //  0: pin low
    //  1: pin high
    int waitResult = 0;
    // waitResult:
    // -1: error
    //  0: timeout
    //  1: event
    while (runThreads && lineGetResult >= 0 && setDBusPropOk && waitResult >= 0)
    {
        // in case of an event or full period round
        if (waitResult > 0 || ticks == 0)
        {
            // Start a new period, no matter if triggered by the
            // last one or an event
            ticks = 0;
            lineGetResult = gpiod_line_get_value(line);
            if (lineGetResult >= 0)
            {
                setDBusPropOk =
                    setDBusProperty(dbusInterface, pinName, chipName, pinNum,
                                    lineGetResult != 0);
            }
            else
            {
                int lastErrno = errno;
                stringstream funcall;
                funcall << "gpiod_line_get_value(<" << chipName << " " << pinNum
                        << ">)";
                logLibgpioCallError(funcall, lineGetResult, lastErrno, pinName,
                                    chipName, pinNum);
            }
        }
        if (lineGetResult >= 0 && setDBusPropOk)
        {
            waitResult = gpiod_line_event_wait(line, &timeout);
            if (waitResult > 0)
            {
                // Use it only to clear the event flags, the
                // actual values of the pins will be obtained by
                // 'gpiod_line_get_value'
                gpiod_line_event_read(line, &event);
            }
            else if (waitResult < 0)
            {
                int lastErrno = errno;
                stringstream funcall;
                funcall << "gpiod_line_event_wait(<" << chipName << " "
                        << pinNum << ">, " << lineEventWaitTimeoutNs << " ns)";
                logLibgpioCallError(funcall, waitResult, lastErrno, pinName,
                                    chipName, pinNum);
            }
            // otherwise timeout
        }
        // if condition not met the loop will end in next iteration
        ticks = (ticks + 1) % readPeriodTicks;
    }
    // If the loop exited for any other reason than globally stopped threads
    // then globally stop the threads.
    if (runThreads)
    {
        stopService(1);
    }
}

/**
 * @brief Block until all the all threads in @threads finished execution
 *
 * After the function returns all threads in @threads are no longer joinable.
 *
 * @param[in,out] threads
 */
void finishThreads(vector<thread>& threads)
{
    for (auto i = 0u; i < threads.size(); ++i)
    {
#ifdef ENABLE_GSH_LOGS
        {
            stringstream ss;
            ss << "Waiting for thread #" << i << endl;
            log<level::INFO>(ss.str().c_str());
        }
#endif
        threads[i].join();
    }
}

/**
 * Start a thread for each key in @dbusPropMapLineObj monitoring the associated
 * gpio pin. Append the @thread object at the end of the @threads. All threads
 * in @threads are joinable.
 *
 * @param[out] threads
 * @param[in] dbusInterface
 * @param[in] dbusPropMapLineObj
 * @param[in] gpioConfig
 */
void startThreads(vector<thread>& threads,
                  shared_ptr<sdbusplus::asio::dbus_interface> dbusInterface,
                  const map<string, gpiod_line_t*>& dbusPropMapLineObj,
                  const GpioJsonConfig& gpioConfig)
{
#ifdef ENABLE_GSH_LOGS
    log<level::INFO>("Starting gpio pins monitoring threads");
#endif
    runThreads = true;
    if (!dbusPropMapLineObj.empty())
    {
        for (auto it = dbusPropMapLineObj.cbegin();
             it != dbusPropMapLineObj.cend(); ++it)
        {
            string pinName = it->first;
            gpiod_line_t* line = it->second;
            gpiod_chip_t* chip = gpiod_line_get_chip(line);
            string chipName = gpiod_chip_name(chip);
            unsigned pinNum = gpiod_line_offset(line);
            double readPeriodSec =
                gpioConfig
                    .getConfig()[pinName][GpioJsonConfig::configKeyReadPeriod];
            uint64_t readPeriodTicks =
                max((uint64_t)1,
                    (uint64_t)(readPeriodSec * 1e9 / lineEventWaitTimeoutNs));
#ifdef ENABLE_GSH_LOGS
            {
                stringstream ss;
                ss << "Setting up thread for:" << endl;
                ss << "  pin_name = " << pinName << endl;
                ss << "  " << GpioJsonConfig::configKeyGpioPin << " = "
                   << pinNum << endl;
                ss << "  " << GpioJsonConfig::configKeyGpioChip << " = "
                   << chipName << endl;
                ss << "  " << GpioJsonConfig::configKeyReadPeriod << " = "
                   << readPeriodSec << endl;
                ss << "  readPeriodTicks = " << readPeriodTicks;
                logPinOperation<level::INFO>(ss.str().c_str(), pinName,
                                             chipName, pinNum);
            }
#endif
            threads.push_back(thread(syncAlertGpioPin, dbusInterface, line,
                                     chipName, pinName, pinNum,
                                     readPeriodTicks));
#ifdef ENABLE_GSH_LOGS
            log<level::INFO>("Thread started");
#endif
        }
    }
    else // ! !dbusPropMapLineObj.empty()
    {
        log<level::WARNING>("No gpio pins monitored");
    }
}

/**
 * @brief Create the DBus object containing the properties corresponding to the
 * monitored gpio pins
 *
 * @param[out] io
 * @param[in] gpioConfig
 *
 * @return A pointer to the dbus interface with the properties set, all boolean,
 * corresponding to the attribute names in @gpioConfig.getConfig().
 */
shared_ptr<sdbusplus::asio::dbus_interface>
    createDbusObject(boost::asio::io_context& io,
                     const GpioJsonConfig& gpioConfig)
{
    auto conn = make_shared<sdbusplus::asio::connection>(io);
#ifdef ENABLE_GSH_LOGS
    {
        stringstream ss;
        ss << "Registering DBus name '" << dbusServiceName << "'";
        log<level::INFO>(ss.str().c_str());
    }
#endif
    conn->request_name(dbusServiceName);
    auto server = sdbusplus::asio::object_server(conn);
    auto dbusInterface =
        server.add_interface(dbusObjectPath, dbusInterfaceName);
    for (auto it = gpioConfig.getConfig().cbegin();
         it != gpioConfig.getConfig().cend(); ++it)
    {
        dbusInterface->register_property(
            it.key(), (bool)it.value()[GpioJsonConfig::configKeyInitialPinVal],
            sdbusplus::asio::PropertyPermission::readOnly);
    }
    dbusInterface->initialize();
    return dbusInterface;
}

void showLastThreadException()
{
    try
    {
        if (lastThreadException != NULL)
        {
            std::rethrow_exception(lastThreadException);
        }
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Exception caught in a gpio monitoring thread",
                        entry("EXCEPTION=%s", e.what()));
    }
}

/**
 * @brief The entry point of the service
 *
 * Read the json configuration file from the path provided as the first
 * argument. Print the help if no argument provided. Expected format:
 *
 * {
 *   "I2C3_ALERT" : {
 *     "gpio_chip" : 0,
 *     "gpio_pin" : 108,
 *     "initial" : false,
 *     "read_period_sec" : 0.5
 *   },
 *   "I2C4_ALERT" : {
 *     "gpio_chip" : 0,
 *     "gpio_pin" : 109,
 *     "initial" : false,
 *     "read_period_sec" : 4
 *   }
 * }
 *
 * Stop the program if config is malformed.
 *
 * Open all the chips specified in the config ("/dev/gpiochip0" in this case).
 * Request all the specified gpio lines (108 and 109), blocking their usage for
 * other applications, such that:
 *
 *   $ gpioinfo
 *   gpiochip0 - 208 lines:
 *   ...
 *      line 105:      unnamed       unused   input  active-high
 *      line 106:      unnamed       unused   input  active-high
 *      line 107: "fpga_ready"    "gpiomon"   input  active-high [used]
 *      line 108:      unnamed "I2C3_ALERT"   input  active-high [used]
 *      line 109:      unnamed "I2C4_ALERT"   input  active-high [used]
 *      line 110:      unnamed       unused   input  active-high
 *      line 111:      unnamed       unused   input  active-high
 *   ...
 *
 * Stop the program if any of those operations failed.
 *
 * Create the DBus service "xyz.openbmc_project.GpioStatusHandler" providing
 * DBus object "/xyz/openbmc_project/GpioStatusHandler" implementing
 * "xyz.openbmc_project.GpioStatus" interface reflecting the state of the gpio
 * pins, such that:
 *
 *   $ busctl introspect xyz.openbmc_project.GpioStatusHandler \
 *       /xyz/openbmc_project/GpioStatusHandler \
 *       xyz.openbmc_project.GpioStatus
 *
 *   NAME            TYPE      SIGNATURE RESULT/VALUE FLAGS
 *   .I2C3_ALERT     property  b         false        emits-change
 *   .I2C4_ALERT     property  b         false        emits-change
 *
 * Stop the program if the DBus service name could not be requested or any other
 * DBus error occured.
 *
 * Start the DBus server thread and a monitoring thread per every pin specified
 * in the config (2 in this case). Run forever reflecting the state of the pins
 * on the "xyz.openbmc_project.GpioStatus" interface:
 *
 *   0 -> false
 *   1 -> true
 *
 * Stop the service althogether if any reading operation on the gpio line
 * failed.
 *
 *
 * All the output of the program goes to 'systemd-journald'. Use
 *
 *   $ journalctl --unit=xyz.openbmc_project.GpioStatusHandler -p 0..7 --follow
 *
 * to catch it live. The complete set of custom tags for the structured messages
 * is:
 *
 * - FILE :: relevant to the json parsing,
 *
 * - FUNCALL, RESULT, ERRNO, ERRNO_STR :: used in any erroneous 'libgpiod'
 * function call,
 *
 * - GPIO_CHIP, GPIO_PIN_NAME, GPIO_PIN_NUM :: used in any operation on the
 * line, successful or not. Their values correspond directly to the config
 * entry:
 *
 *   - GPIO_CHIP <=> entry's "gpio_chip" property with the "gpiochip" prefix (ie
 * '0' <-> "gpiochip0"),
 *
 *   - GPIO_PIN_NAME <=> entry's key (ie "I2C3_PIN" <=> "I2C3_PIN"),
 *
 *   - GPIO_PIN_NUM <=> entry's "gpio_pin" property (ie '108' <=> '108'),
 *
 * - EXCEPTION :: universal.
 *
 * The only message levels currently used are INFO, WARNING and ERR. The last
 * one always precedes the termination of the program, and the internally caused
 * termination of the program should always be accompanied by ERR entry.
 *
 * Taking above into consideration the output of the following command should
 * contain all the output the service produces:
 *
 *   $ journalctl --unit=xyz.openbmc_project.GpioStatusHandler \
 *       --output-fields=PRIORITY,MESSAGE,FILE,FUNCALL,RESULT,ERRNO,ERRNO_STR,\
 *                       GPIO_CHIP,GPIO_PIN_NAME,GPIO_PIN_NUM,EXCEPTION \
 *       -o verbose -p info
 *
 * @return Non-zero in case of error, zero otherwise. Currently the service
 * doesn't implement any execution path which is finite and not erroneous, so
 * the return code is always nonzero.
 */
int main(int argc, char* argv[])
{
    int mainResult;
    if (argc > 1)
    {
        string fileName = argv[1];
        try
        {
            GpioJsonConfig gpioConfig(fileName);

            shared_ptr<sdbusplus::asio::dbus_interface> dbusInterface =
                createDbusObject(io, gpioConfig);

            GpioChips gpioChips(gpioConfig);

            GpioLines gpioLines(gpioChips, gpioConfig);

            vector<thread> threads;
            startThreads(threads, dbusInterface,
                         gpioLines.getDbusPropMapLineObj(), gpioConfig);

            // Nested try/catch so that opened lines in
            // between could be closed gracefully.
            try
            {
#ifdef ENABLE_GSH_LOGS
                log<level::INFO>("Starting DBus server");
#endif
                io.run();
#ifdef ENABLE_GSH_LOGS
                log<level::INFO>("DBus server closed");
#endif
            }
            catch (const std::exception& e)
            {
                log<level::ERR>(
                    "Exception caught from DBus server dispatch loop",
                    entry("EXCEPTION=%s", e.what()));
                stopService(1);
            }

#ifdef ENABLE_GSH_LOGS
            log<level::INFO>("Waiting for gpio monitoring threads to finish");
#endif
            finishThreads(threads);
            showLastThreadException();
            mainResult = threadsExitCode;
        }
        catch (const json::exception& e)
        {
            stringstream ss;
            ss << "Exception caught while parsing the json file. "
               << "Configuration file format expected: " << endl
               << GpioJsonConfig::expectedJsonConfigFormat;
            log<level::ERR>(ss.str().c_str(),
                            entry("FILE=%s", fileName.c_str()),
                            entry("EXCEPTION=%s", e.what()));
            mainResult = 1;
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("Exception caught",
                            entry("EXCEPTION=%s", e.what()));
            mainResult = 1;
        }
    }
    else // ! argc > 1
    {
        stringstream ss;
        ss << "A json configuration file expected as the first argument "
           << "in the format: " << endl
           << GpioJsonConfig::expectedJsonConfigFormat;
        log<level::ERR>(ss.str().c_str());
        mainResult = 1;
    }
    return mainResult;
}
