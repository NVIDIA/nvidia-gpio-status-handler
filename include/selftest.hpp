
/*
 *
 */

#pragma once

#include "aml.hpp"
#include "dat_traverse.hpp"
#include "event_handler.hpp"

#include <nlohmann/json.hpp>

#include <ostream>
#include <string>
#include <vector>

namespace selftest
{

/**
 * @brief TestPointResult contains single test point result attributes
 */
struct TestPointResult
{
    /** @brief test point name, eg. "PGOOD", "DEVICE_HSC0" **/
    std::string targetName;

    /** @brief value read from accessor, eg. "OK", "HW_PIN_ABNORMAL_HIGH" **/
    std::string valRead;

    /** @brief DAT configured expected value, eg. "OK" **/
    std::string valExpected;

    /** @brief true when valRead equals valExpected **/
    bool result;
};

/**
 * @brief DeviceResult contains multiple test layer results of multiple TP
 * results
 */
struct DeviceResult
{
    /**
     * @brief eg. maps "power_rail" test layer onto its test points results
     * vector as there can be many TP's
     */
    std::map<std::string, std::vector<TestPointResult>> layer;
};

/**
 * @brief ReportResult is a container of multiple DeviceResults and its
 * underlying containers
 * @note  eg. DEV    |  layer      | test point  | testpoint result
 *            GPU0  -> power_rail
 *                                -> TP_PGOOD   -> (tp_result)
 *                                -> EXAMPLE_TP -> (tp_result)
 *                  -> pin_status
 *                                -> EXAMPLE_TP -> (...)
 *                  -> (...)
 *            HSC0  -> power_rail -> (...)
 *                  -> (...)
 *            (...)
 */
using ReportResult = std::map<std::string, DeviceResult>;

/**
 * @brief A class to generate aggregated report from ReportResult.
 */
class Report
{
  public:
    Report() : tpTotal(0), tpFailed(0) {}
    ~Report() = default;

  public:
    /**
     * @brief Generates internal json report.
     *
     * @param[in] reportRes
     *
     * @return true when report successfuly generated, otherwise false
     */
    bool generateReport(ReportResult& reportRes);

    /**
     * @brief Returns internal json report object.
     *
     * @return json
     */
   const nlohmann::ordered_json& getReport(void);

    /**
     * @brief Output internal json report. Has to be generated first.
     *
     * @param os
     * @param rpt
     * @return std::ostream&
     */
    friend std::ostream& operator<<(std::ostream& os, const Report& rpt);

  private:
    /**
     * @brief Writes header data during report generation. Must be called in 
     *        the end of report generation.
     */
    void writeSummaryHeader(void);

    /**
     * @brief used to process a layer's testpoints for a given device
     *
     * @param[out] jdev device to write given layer report piece to
     * @param[in] layerName layer key name (its support is validated internally)
     * @param[in] testpoints TPs results of given layer to process
     * 
     * @return returns true on success, false on unsupported layer key
     */
    bool processLayer(nlohmann::ordered_json& jdev, 
                          const std::string& layerName,
                          std::vector<selftest::TestPointResult>& testpoints);

    /** @brief Internal report storage. **/
    nlohmann::ordered_json _report;

    /** @brief Total test points number (counted during report generation) **/
    uint32_t tpTotal;

    /** @brief Failed test points count (counted during report generation) **/
    uint32_t tpFailed;
};

/**
 * @brief A class to perform selftest.
 */
class Selftest : public event_handler::EventHandler
{
  public:
    Selftest(const std::string& name,
             const std::map<std::string, dat_traverse::Device>& dat);
    ~Selftest() = default;

  public:
    /**
     * @brief Updates device health on the DBUS in GpuMgr tree
     *
     * @param[in] device - name of device
     * @param[in] health - health status of device
     */
    void updateDeviceHealth(const std::string& device,
                            const std::string& health);

    /**
     * @brief Aggregates selftest results of report->devices->testpoints
     * to simple ok or not ok.
     *
     * @param[in] reportRes
     * @return true - all TP in report passed, false - any TP failed
     */
    bool evaluateTestReport(const ReportResult& reportRes);

    /**
     * @brief Checks if a device report is already in report container.
     *
     * @param[in] devName device name to check presence in reportRes
     * @param[in] reportRes report container to check in for devName
     * @return true - entry already in reportRes, false - entry absent
     */
    bool isDeviceCached(const std::string& devName,
                        const ReportResult& reportRes);

    /**
     * @brief Performs selftest on device extracted from event and returns
     * already evaluated test report corectness, not test operation status.
     *
     * @param[in] event
     * @return aml::RcCode::succ when all testpoints passed, otherwise
     * aml::RcCode::error (failed TP or failed test operation)
     */
    aml::RcCode process([[maybe_unused]] event_info::EventNode& event) override
    {
        if (_dat.count(event.device) == 0)
        {
            std::cerr << "Error: device: " << event.device
                      << " is an invalid key!" << std::endl;
            return aml::RcCode::error;
        }

        ReportResult rep;
        const dat_traverse::Device& dev = _dat.at(event.device);

        if (perform(dev, rep) != aml::RcCode::succ)
        {
            return aml::RcCode::error;
        }

        if (!evaluateTestReport(rep))
        {
            return aml::RcCode::error;
        }

        return aml::RcCode::succ;
    };

    /** @brief Performs selftest on given device.
     *
     * @note the report container can be reused: as a cache to omit non unique
     * device tests; to stack multiple but not associated by TP devices for
     * report generation;
     * @param[in]  dev        - device to perform test on
     * @param[out] reportRes  - container with written in TP's results @ref
     * <ReportResult>
     * @return aml::RcCode meaning testing operation status, not test results
     */
    aml::RcCode perform(const dat_traverse::Device& dev,
                        ReportResult& reportRes);

    /** @brief Performs selftest on entire DAT.
     *
     * @param[out] reportRes  - container with written in TP's results @ref
     * <ReportResult>
     * @return aml::RcCode meaning testing operation status, not test results
     */
    aml::RcCode performEntireTree(ReportResult& reportRes);

    /**
     * @brief Checks selftest result of particular device -> testpoints
     *
     * @param[in] deviceResult
     * @return true - all TP in *device* passed, false - any TP failed
     */
    static bool evaluateDevice(const DeviceResult& deviceResult);

  private:
    /** @brief Internal DAT reference. **/
    const std::map<std::string, dat_traverse::Device>& _dat;
};

} // namespace selftest

namespace event_handler
{

/**
 * @brief estimates if a device is implemented (in dbus)
 * @note TODO: to refactor, uses duplicated function from message composer
 *
 * @param[in] device - device name to check presence
 *
 * @return true when device possibly exists and is implemented in dbus,
 * otherwise false
 */
bool checkDeviceDBus(const std::string& device);

/**
 * @brief A class for finding the potential root cause of problem when an event
 * is received, it tests device and it associated devices through device
 * association tree.
 *
 */
class RootCauseTracer : public EventHandler
{
  public:
    RootCauseTracer(const std::string& name,
                    std::map<std::string, dat_traverse::Device>& dat,
                    std::function<bool(const std::string&)> checkDeviceExists) :
        EventHandler(name),
        _dat(dat),
        checkDeviceExists(checkDeviceExists)
    {}

    ~RootCauseTracer() = default;

  public:
    /**
     * @brief does selftest on device taken from event and associated devices;
     * finds root cause of failure and updates health and origin of condition
     * of event device. Writes back test report to event.
     *
     * @param[in out] event - shall carry problematic device name; gets written
     * in selftest report of problematic device + its associated devices
     *
     * @return aml::RcCode::succ when performed root cause tracing, otherwise
     * aml::RcCode::error (wrong device name in event, performing selftest
     * failed). Warning - does not mean a root cause was found, but op success.
     */
    aml::RcCode process([[maybe_unused]] event_info::EventNode& event) override;

  private:
    /**
     * @brief updates device root cause of failure
     *
     * @param[out] dev  - device to update health and origin of condition
     * @param[in] rootCauseDevice - device which failed selftest
     * @param[in] selftester - self test object
     */
    void updateRootCause(dat_traverse::Device& dev,
                        dat_traverse::Device& rootCauseDevice,
                        selftest::Selftest& selftester);

    /**
     * @brief finds most nested device that caused the problem
     *
     * @param[in] triggeringDevice - device that root cause search started from
     * @param[in] report - report result of selftest performed on 
     * @triggeringDevice
     * @param[out] rootCauseDevice - returns failed device name if found,
     * otherwise empty
     * 
     * @return true when found root cause, otherwise false (whole report correct)
     */
    bool findRootCause(const std::string& triggeringDevice, 
                      const selftest::ReportResult& report,
                      std::string& rootCauseDevice);

    /** @brief Internal DAT reference. **/
    std::map<std::string, dat_traverse::Device>& _dat;
    
    /** @brief Used to check if device exists and is implemented and is present
     * on DBUS. **/
    std::function<bool(const std::string&)> checkDeviceExists;
};

} // namespace event_handler
