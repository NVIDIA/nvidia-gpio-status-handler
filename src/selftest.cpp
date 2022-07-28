
/*
 *
 */

#include "selftest.hpp"

#include "dbus_accessor.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/property.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/exception.hpp>

#include <chrono>
#include <iostream>
#include <variant>
#include <queue>

namespace selftest
{

static constexpr auto reportResultPass = "Pass";
static constexpr auto reportResultFail = "Fail";

using phosphor::logging::entry;
using phosphor::logging::level;
using phosphor::logging::log;

void Selftest::updateDeviceHealth(const std::string& device,
                                  const std::string& health)
{
    try
    {
        auto bus = sdbusplus::bus::new_default_system();
        auto method = bus.new_method_call("xyz.openbmc_project.ObjectMapper",
                                          "/xyz/openbmc_project/object_mapper",
                                          "xyz.openbmc_project.ObjectMapper",
                                          "GetSubTree");
        method.append("/xyz/openbmc_project/inventory/system");
        method.append(2);
        method.append(std::vector<std::string>());

        using GetSubTreeType = std::vector<std::pair<
            std::string,
            std::vector<std::pair<std::string, std::vector<std::string>>>>>;

        auto reply = bus.call(method);
        GetSubTreeType subtree;
        reply.read(subtree);

        for (auto& objPath : subtree)
        {
            if (boost::algorithm::ends_with(objPath.first, "/" + device))
            {
                std::string healthState =
                    "xyz.openbmc_project.State.Decorator.Health.HealthType." +
                    health;
                    
                log_dbg("Setting Health Property for: %s healthState: %s\n", objPath.first, healthState);
                bool ok = dbus::setDbusProperty(objPath.first,
                    "xyz.openbmc_project.State.Decorator.Health", "Health",
                    PropertyVariant(healthState));
                if (ok == true)
                {
                    log_dbg("Changed health property as expected\n");
                } // else setDbusProperty() prints message on std::cerr
            }
        }
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        std::cerr << "ERROR WITH SDBUSPLUS BUS " << e.what() << "\n";
        log<level::ERR>("Failed to establish sdbusplus connection",
                        entry("SDBUSERR=%s", e.what()));
    }
}
    
bool Selftest::evaluateDevice(const DeviceResult& deviceResult)
{
    for (auto& layer : deviceResult.layer)
    {
        if (std::any_of(layer.second.begin(), layer.second.end(),
                        [](auto tp) { return !tp.result; }))
        {
            return false;
        }
    }

    return true;
}

bool Selftest::evaluateTestReport(const ReportResult& reportRes)
{
    for (auto& dev : reportRes)
    {
        if (!evaluateDevice(dev.second))
        {
            return false;
        }
    }
    
    return true;
}

bool Selftest::isDeviceCached(const std::string& devName,
                              const ReportResult& reportRes)
{
    if (reportRes.find(devName) == reportRes.end())
    {
        return false; /* not cached */
    }

    return true; /* already cached */
}

aml::RcCode Selftest::perform(const dat_traverse::Device& dev,
                              ReportResult& reportRes)
{
    if (isDeviceCached(dev.name, reportRes))
    {
        return aml::RcCode::succ; /* early exit, non unique device test */
    }

    auto fillTpRes = [](selftest::TestPointResult& tp,
                        const std::string& expVal, const std::string& readVal,
                        const std::string& name) {
        tp.targetName = name;
        tp.valExpected = expVal;
        // TODO: clean dependency of "123" return on read fail
        if (readVal == "123")
        {
            tp.valRead = "Error - TP read failed.";
            tp.result = false;
        }
        else
        {
            tp.valRead = readVal;
            // in case of empty expected value default to positive result
            tp.result = (expVal.size() == 0) ? true : readVal == expVal;
        }
    };

    auto& availableLayers = dev.test;
    DeviceResult tmpDeviceReport;
    /* Important, preinsert new device test to detect recursed device. */
    reportRes[dev.name] = tmpDeviceReport;

    for (auto& tl : availableLayers)
    {
        auto& tmpLayerReport = tmpDeviceReport.layer[tl.first];

        for (auto& tp : tl.second.testPoints)
        {
            auto& testPoint = tp.second;
            auto acc = testPoint.accessor;
            TestPointResult tmpTestPointResult;

            if (acc.isValidDeviceAccessor())
            {
                const std::string& devName = acc.read();
                if (this->_dat.count(devName) == 0)
                {
                    std::cerr << "Error: invalid device key: " << devName
                              << " in nested tp in selftest perform"
                              << std::endl;
                    return aml::RcCode::error;
                }

                auto& devNested = this->_dat.at(devName);
                aml::RcCode selftestRes = aml::RcCode::succ;
                if (!isDeviceCached(devName, reportRes))
                {
                    selftestRes = this->perform(devNested, reportRes);
                }

                bool devEvalRes = (selftestRes == aml::RcCode::succ) && 
                                    evaluateDevice(reportRes[devName]);

                fillTpRes(tmpTestPointResult, testPoint.expectedValue,
                         (devEvalRes) ? testPoint.expectedValue
                                      : reportResultFail, devName);
            }
            else
            {
                auto accRead = acc.read();
                fillTpRes(tmpTestPointResult, testPoint.expectedValue, accRead,
                          tp.first);
            }
            tmpLayerReport.insert(tmpLayerReport.begin(), tmpTestPointResult);
        }
    }
    reportRes[dev.name] = tmpDeviceReport;
    return aml::RcCode::succ;
}

aml::RcCode Selftest::performEntireTree(ReportResult& reportRes)
{
    for (auto& dev : _dat)
    {
        if (perform(dev.second, reportRes) != aml::RcCode::succ)
        {
            return aml::RcCode::error;
        }
    }

    return aml::RcCode::succ;
}

Selftest::Selftest(const std::string& name,
                   const std::map<std::string, dat_traverse::Device>& dat) :
    event_handler::EventHandler(name),
    _dat(dat){};

/* ========================= report ========================= */

static std::map<std::string, std::string> layerToKeyLut = 
{
    {"power_rail", "power-rail-status"},
    {"erot_control", "erot-control-status"},
    {"pin_status", "pin-status"},
    {"interface_status", "interface-status"},
    {"firmware_status", "firmware-status"},
    {"protocol_status", "protocol-status"},
    {"data_dump", "data-dump"}
};

std::ostream& operator<<(std::ostream& os, const Report& rpt)
{
    os << rpt._report.dump(4);
    return os;
}

static std::string getTimestampString(void)
{
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&time);

    std::stringstream stream;
    stream << std::put_time(tm, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

bool Report::processLayer(nlohmann::ordered_json& jdev, 
                          const std::string& layerName,
                          std::vector<selftest::TestPointResult>& testpoints)
{
    if (layerToKeyLut.count(layerName) == 0)
    {
        std::cerr << "Error: generate report invalid layer key: "
                          << layerName << std::endl;
        return false;
    }

    const auto layerKey = layerToKeyLut.at(layerName);
    auto layerPassOrFail = true;
    jdev[layerKey]["test-points"] = nlohmann::json::array();

    for (auto& tp : testpoints)
    {
        this->tpTotal++;
        if (!tp.result)
        {
            this->tpFailed++;
            layerPassOrFail = false;
        }

        if (tp.valExpected.size() == 0)
        {
            jdev[layerKey]["test-points"] +=
                {{"name", tp.targetName}, {"value", tp.valRead}};
        }
        else
        {
            auto resStr = tp.result ? reportResultPass : reportResultFail;
            jdev[layerKey]["test-points"] +=
                {{"name", tp.targetName},
                {"value", tp.valRead},
                {"value-expected", tp.valExpected},
                {"result", resStr}};
        }
    }

    jdev[layerKey]["result"] = 
                    layerPassOrFail ? reportResultPass : reportResultFail;

    return true;
}

bool Report::generateReport(ReportResult& reportRes)
{
    auto currentTimestamp = getTimestampString();
    this->tpTotal = 0;
    this->tpFailed = 0;

    /* preinsert primary keys to force keys order */
    this->_report["header"] = nlohmann::ordered_json::object();
    this->_report["tests"] = nlohmann::ordered_json::array();

    for (auto& dev : reportRes)
    {
        auto& devTestLayers = dev.second.layer;
        nlohmann::ordered_json jdev;
        jdev["device-name"] = dev.first;
        jdev["firmware-version"] = "<todo>";
        jdev["timestamp"] = currentTimestamp;

        for (auto& layer : devTestLayers)
        {
            /* to keep this key last, skip here and handle explicitly last */
            if (layer.first == "data_dump")
            {
                continue; 
            }

            if (!processLayer(jdev, layer.first, layer.second))
            {
                return false;
            }
        }

        auto dataDumpLayer = devTestLayers.find("data_dump");
        if (dataDumpLayer != devTestLayers.end())
        {
            if (!processLayer(jdev, dataDumpLayer->first, 
                                    dataDumpLayer->second))
            {
                return false;
            }
        }

        this->_report["tests"] += (nlohmann::ordered_json)jdev;
    }

    this->writeSummaryHeader();

    return true;
}

const nlohmann::ordered_json& Report::getReport(void)
{
    return this->_report;
}

void Report::writeSummaryHeader(void)
{
    this->_report["header"]["name"] = "Self test report";
    this->_report["header"]["version"] = "1.0";
    this->_report["header"]["timestamp"] = getTimestampString();
    this->_report["header"]["summary"]["test-case-total"] = this->tpTotal;
    this->_report["header"]["summary"]["test-case-failed"] = this->tpFailed;
}

/* ========================= free function todo ========================= */

aml::RcCode DoSelftest([[maybe_unused]] const dat_traverse::Device& dev,
                       [[maybe_unused]] const std::string& report)
{
    return aml::RcCode::error;
}

} // namespace selftest

/* ========================= RootCauseTracer ========================= */

namespace event_handler
{

/* TODO: cleanup, this method duplicated here and in message composer 
    used to check if device exists (present on dbus) */
static std::string getDeviceDBusPath(const std::string& device)
{

    auto bus = sdbusplus::bus::new_default_system();
    auto method = bus.new_method_call("xyz.openbmc_project.ObjectMapper",
                                        "/xyz/openbmc_project/object_mapper",
                                        "xyz.openbmc_project.ObjectMapper",
                                        "GetSubTreePaths");
    int depth = 6;
    method.append("/xyz/openbmc_project", depth,
                    std::vector<std::string>());
    auto reply = bus.call(method);
    std::vector<std::string> dbusPaths;
    reply.read(dbusPaths);

    for (auto& objPath : dbusPaths)
    {
        if (boost::algorithm::ends_with(objPath, "/" + device))
        {
            return objPath;
        }
    }

    std::cerr << "Found no path in ObjectMapper ending with device: "
                << device << "\n";
    return "";
}

bool checkDeviceDBus(const std::string& device)
{
    auto devPath = getDeviceDBusPath(device);
    return !devPath.empty();
}

bool RootCauseTracer::findRootCause(const std::string& triggeringDevice, 
                                    const selftest::ReportResult& report,
                                    std::string& rootCauseDevice)
{
    rootCauseDevice.clear();

#if ROOT_CAUSE_TRACER_USE_ONLY_ASSOCIATION_KEY_TRAVERSAL
    auto childVec = DATTraverse::getSubAssociations(_dat, triggeringDevice);
#else // use DAT testpoints traversal
    auto childVec = DATTraverse::getSubAssociations(_dat, triggeringDevice, 
                                                    true);
#endif// #if ROOT_CAUSE_TRACER_USE_ONLY_ASSOCIATION_KEY_TRAVERSAL

    /* test every device in order from most nested to least; 
    ignore devices not yet implemented in DBUS (other logic dependence);
    first failed device is the root cause of condition for device triggering the
    tracing */
    for (auto it = childVec.rbegin(); it != childVec.rend(); it++)
    {
        if (!checkDeviceExists(*it))
        {
//            std::cout << "device " << *it << " not found on dbus" << "\n";
            continue;
        }

        if (!selftest::Selftest::evaluateDevice(report.at(*it)))
        {
            rootCauseDevice = *it;
            return true;
        }
    }
    
    return false;
}

void RootCauseTracer::updateRootCause(dat_traverse::Device& dev,
                                dat_traverse::Device& rootCauseDevice,
                                selftest::Selftest& selftester)
{
    dat_traverse::Status status;
    status.health = "Critical";
    status.healthRollup = "Critical";
    status.originOfCondition = rootCauseDevice.name;
    status.triState = "Error";
    DATTraverse::setHealthProperties(dev, status);
    DATTraverse::setOriginOfCondition(dev, status);
    selftester.updateDeviceHealth(dev.name, status.health);
}

aml::RcCode
    RootCauseTracer::process([[maybe_unused]] event_info::EventNode& event)
{
    std::string problemDevice = event.device;
    if ((problemDevice.length() == 0) || (_dat.count(problemDevice) == 0))
    {
        std::cerr << "Error: rootCauseTracer device: [" << problemDevice
                  << "] is invalid key - cannot process rootCause" << std::endl;
        return aml::RcCode::error;
    }

    selftest::ReportResult completeReportRes;
    selftest::Selftest selftester("rootCauseSelftester", _dat);
    std::string rootCauseCandidateName {};

    if (selftester.perform(_dat.at(problemDevice), completeReportRes) != aml::RcCode::succ)
    {
        std::cerr << "Error: rootCauseTracer failed to perform selftest "
                    << "for device " << problemDevice << std::endl;
        return aml::RcCode::error;
    }

    if (findRootCause(problemDevice, completeReportRes, rootCauseCandidateName))
    {
        updateRootCause(_dat.at(problemDevice), _dat.at(rootCauseCandidateName), 
                        selftester);
    }

    selftest::Report reportGenerator;
    if (!reportGenerator.generateReport(completeReportRes))
    {
        std::cerr << "Error: rootCauseTracer failed to generate report!"
                  << std::endl;
        return aml::RcCode::error;
    }

    event.selftestReport = reportGenerator.getReport();

    return aml::RcCode::succ;
}

} // namespace event_handler
