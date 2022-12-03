#include "nlohmann/json.hpp"
#include "selftest.hpp"

#include <iostream>

#include "gmock/gmock.h"

/**
 * @brief populates map but takes json object as an input instead of filepath
 */
void populateMapMock(std::map<std::string, dat_traverse::Device>& dat,
                     const nlohmann::json& j)
{
    for (const auto& el : j.items())
    {
        auto deviceName = el.key();
        dat_traverse::Device device(deviceName, el.value());
        dat.insert(
            std::pair<std::string, dat_traverse::Device>(deviceName, device));
    }

    // fill out parents of all child devices on 2nd pass
    for (const auto& entry : dat)
    {
        for (const auto& child : entry.second.association)
        {
            dat.at(child).parents.push_back(entry.first);
        }
    }
}

/**
 * @brief Convenience cout print for test point debugs
 */
std::ostream& operator<<(std::ostream& os, const selftest::TestPointResult& res)
{
    return os << "[" << res.targetName << "] exp [" << res.valExpected
              << "] read [" << res.valRead << "] res ["
              << (res.result ? "true" : "false") << "]";
}

/**
 * @brief Convenience cout print for report debugs
 */
std::ostream& operator<<(std::ostream& os, const selftest::ReportResult& report)
{
    os << "TEST REPORT CONTENT:\n";
    for (auto& dev : report)
    {
        os << "device: [" << dev.first << "]\n";
        for (auto& layer : dev.second.layer)
        {
            os << "\tlayer: [" << layer.first << "]\n";
            for (auto& tp : layer.second)
            {
                os << "\t\t" << tp << "\n";
            }
        }
    }
    return os;
}

static const std::list<std::string> layers = {
    "power_rail",       "erot_control",    "pin_status",
    "interface_status", "protocol_status", "firmware_status"};

static nlohmann::json
    json_testpoint_template(const std::string& tp_name = "",
                            bool is_accessor_device = false,
                            const std::string& opt_dev_name = "")
{
    static int template_id = 0;
    nlohmann::json j;
    j["name"] = "TP" + std::to_string(template_id++) + "_" + tp_name;
    j["expected_value"] = "some_data";

    if (is_accessor_device)
    {
        j["accessor"]["type"] = "DEVICE";
        j["accessor"]["device_name"] = opt_dev_name;
        return j;
    }

    j["accessor"]["type"] = "TEST";
    j["accessor"]["test_value"] = "some_data";

    return j;
}

static nlohmann::json
    json_device_template([[maybe_unused]] const std::string& dev_name,
                         std::string associated_device = "")
{
    nlohmann::json j;

    j["association"] = nlohmann::json::array();
    if (associated_device.size())
    {
        j["association"] += associated_device;
    }

    j["power_rail"] = nlohmann::json::array();
    j["erot_control"] = nlohmann::json::array();
    j["pin_status"] = nlohmann::json::array();
    j["interface_status"] = nlohmann::json::array();
    j["firmware_status"] = nlohmann::json::array();
    j["protocol_status"] = nlohmann::json::array();
    return j;
}

TEST(selftestTest, MakeCall)
{
    std::map<std::string, dat_traverse::Device> datMap;
    selftest::Selftest selftest("selftestObj", datMap);
    EXPECT_EQ(selftest.getName(), "selftestObj");
}

TEST(selftestTest, isCachedTest)
{
    std::map<std::string, dat_traverse::Device> datMap;
    selftest::Selftest selftest("selftestObj", datMap);

    selftest::ReportResult reportResult;
    reportResult["GPU0"];
    reportResult["GPU1"];
    reportResult["GPU2"];
    reportResult["VR0"];
    reportResult["VR1"];
    reportResult["HSC0"];

    EXPECT_EQ(selftest.isDeviceCached("GPU0", reportResult), true);
    EXPECT_EQ(selftest.isDeviceCached("GPU1", reportResult), true);
    EXPECT_EQ(selftest.isDeviceCached("GPU2", reportResult), true);
    EXPECT_EQ(selftest.isDeviceCached("GPU3", reportResult), false);
    EXPECT_EQ(selftest.isDeviceCached("VR0", reportResult), true);
    EXPECT_EQ(selftest.isDeviceCached("VR1", reportResult), true);
    EXPECT_EQ(selftest.isDeviceCached("VR2", reportResult), false);
    EXPECT_EQ(selftest.isDeviceCached("VR3", reportResult), false);
    EXPECT_EQ(selftest.isDeviceCached("HSC0", reportResult), true);
    EXPECT_EQ(selftest.isDeviceCached("HSC1", reportResult), false);
    EXPECT_EQ(selftest.isDeviceCached("HSC2", reportResult), false);
    EXPECT_EQ(selftest.isDeviceCached("HSC3", reportResult), false);
}

TEST(selftestTest, evalTestReportTest)
{
    /* Input: synthetic reportResult.
       Expected: a) evals as true when all test points passed,
                 b) evals as false when any tp fails. */
    std::map<std::string, dat_traverse::Device> datMap;
    selftest::Selftest selftest("selftestObj", datMap);

    selftest::ReportResult reportResult;
    struct selftest::TestPointResult dummyTp = {.targetName = "sampleTp",
                                                .valRead = "OK",
                                                .valExpected = "OK",
                                                .result = true};

    std::vector<struct selftest::TestPointResult> tpVec;
    tpVec.insert(tpVec.end(), dummyTp);
    tpVec.insert(tpVec.end(), dummyTp);

    struct selftest::DeviceResult dummyDevice;
    dummyDevice.layer["power_rail"] = tpVec;
    dummyDevice.layer["erot_control"] = tpVec;
    dummyDevice.layer["pin_status"] = tpVec;
    dummyDevice.layer["interface_status"] = tpVec;
    dummyDevice.layer["firmware_status"] = tpVec;
    dummyDevice.layer["protocol_status"] = tpVec;

    reportResult["GPU0"] = dummyDevice;
    reportResult["HSC0"] = dummyDevice;

    EXPECT_EQ(selftest.evaluateTestReport(reportResult), true);

    /* overwrite single TP to be failed */
    reportResult["HSC0"].layer["power_rail"][0].result = false;
    reportResult["HSC0"].layer["power_rail"][0].valRead = "fail";

    EXPECT_EQ(selftest.evaluateTestReport(reportResult), false);
}

TEST(selftestTest, doPerform)
{
    /*  Device_A: (N dummy TP's) + 1 TP pointing to Device_B.
        Device_B: (N dummy TP's).
        Expected: report of 2 devices, where report of Dev_A
        consists of N TP reports and one device_Device_B TP
        and Dev_B report.
        Expected: a failed tp in *child* device causes corresponding
        tp in *parent* device to be failed too. */
    nlohmann::json jdat;
    nlohmann::json jgpu0;
    nlohmann::json jvr0;
    nlohmann::json jbaseboard;
    jgpu0 = json_device_template("GPU0", "VR0");
    jgpu0["power_rail"] = {json_testpoint_template("PGOOD", true, "VR0")};
    jgpu0["erot_control"] = {json_testpoint_template()};
    jgpu0["pin_status"] = {json_testpoint_template()};
    jgpu0["interface_status"] = {json_testpoint_template()};
    jgpu0["firmware_status"] = {json_testpoint_template()};
    jgpu0["protocol_status"] = {json_testpoint_template()};

    jvr0 = json_device_template("VR0");
    jvr0["power_rail"] = {json_testpoint_template()};
    jvr0["erot_control"] = {json_testpoint_template()};
    jvr0["pin_status"] = {json_testpoint_template()};
    jvr0["interface_status"] = {json_testpoint_template()};
    jvr0["firmware_status"] = {json_testpoint_template()};
    jvr0["protocol_status"] = {json_testpoint_template()};

    jbaseboard = json_device_template("Baseboard");
    jbaseboard["association"] += "GPU0";
    jbaseboard["association"] += "VR0";

    jdat["Baseboard"] = jbaseboard;
    jdat["GPU0"] = jgpu0;
    jdat["VR0"] = jvr0;

    // std::cout << jgpu0.dump(4) << std::endl;
    // std::cout << jvr0.dump(4) << std::endl;
    // std::cout << jbaseboard.dump(4) << std::endl;
    // std::cout << jdat.dump(4) << std::endl;

    std::map<std::string, dat_traverse::Device> datMap;
    populateMapMock(datMap, jdat);
    // dat_traverse::Device::printTree(datMap);
    selftest::Selftest selftest("selftestObj", datMap);
    selftest::ReportResult rep_res;
    EXPECT_EQ(selftest.perform(datMap.at("GPU0"), rep_res), aml::RcCode::succ);
    EXPECT_EQ(rep_res.size(), 2);
    EXPECT_EQ(rep_res.find("GPU0") != rep_res.end(), true);
    EXPECT_EQ(rep_res.find("VR0") != rep_res.end(), true);
    for (const auto& layer : layers)
    {
        EXPECT_EQ(rep_res["GPU0"].layer[layer].size(), jgpu0[layer].size());
        EXPECT_EQ(rep_res["VR0"].layer[layer].size(), jvr0[layer].size());
    }
    // std::cout << rep_res;

    /* purposely fail single TP in child device (VR0) of parent device (GPU0),
       check if parent device (GPU0) has a failed result in VR0 testpoint */
    selftest::ReportResult otherRep;
    auto tp = datMap.at("VR0").test["power_rail"].testPoints.begin();
    tp->second.expectedValue = "force_test_to_fail";
    EXPECT_EQ(selftest.perform(datMap.at("GPU0"), otherRep), aml::RcCode::succ);
    EXPECT_EQ(otherRep["VR0"].layer["power_rail"][0].result, false);
    EXPECT_EQ(otherRep["GPU0"].layer["power_rail"][0].result, false);
}

TEST(selftestTest, performUsingLayerFilter)
{
    /*  Device_A: (N dummy TP's) + 1 TP pointing to Device_B.
        Device_B: (N dummy TP's).
        Perform using "data_dump" and "erot_control" layers filters.
        Expected: report of 2 devices, where:
        * report of Dev_A consists of N TP reports excluding filtered layers and
       one device_Device_B TP
        * Dev_B report similarily to Dev_A has filtered out layers testpoints.
     */
    nlohmann::json jdat;
    nlohmann::json jgpu0;
    nlohmann::json jvr0;
    nlohmann::json jbaseboard;
    jgpu0 = json_device_template("GPU0", "VR0");
    jgpu0["power_rail"] = {json_testpoint_template("PGOOD", true, "VR0")};
    jgpu0["erot_control"] = {json_testpoint_template()};
    jgpu0["pin_status"] = {json_testpoint_template()};
    jgpu0["interface_status"] = {json_testpoint_template()};
    jgpu0["firmware_status"] = {json_testpoint_template()};
    jgpu0["protocol_status"] = {json_testpoint_template()};
    jgpu0["data_dump"] = {json_testpoint_template()};

    jvr0 = json_device_template("VR0");
    jvr0["power_rail"] = {json_testpoint_template()};
    jvr0["erot_control"] = {json_testpoint_template()};
    jvr0["pin_status"] = {json_testpoint_template()};
    jvr0["interface_status"] = {json_testpoint_template()};
    jvr0["firmware_status"] = {json_testpoint_template()};
    jvr0["protocol_status"] = {json_testpoint_template()};
    jvr0["data_dump"] = {json_testpoint_template()};

    jbaseboard = json_device_template("Baseboard");
    jbaseboard["association"] += "GPU0";
    jbaseboard["association"] += "VR0";

    jdat["Baseboard"] = jbaseboard;
    jdat["GPU0"] = jgpu0;
    jdat["VR0"] = jvr0;

    // std::cout << jgpu0.dump(4) << std::endl;
    // std::cout << jvr0.dump(4) << std::endl;
    // std::cout << jbaseboard.dump(4) << std::endl;
    // std::cout << jdat.dump(4) << std::endl;

    std::map<std::string, dat_traverse::Device> datMap;
    populateMapMock(datMap, jdat);
    // dat_traverse::Device::printTree(datMap);
    selftest::Selftest selftest("selftestObj", datMap);
    selftest::ReportResult rep_res;
    EXPECT_EQ(
        selftest.perform(datMap.at("GPU0"), rep_res,
                         std::vector<std::string>{"data_dump", "erot_control"}),
        aml::RcCode::succ);
    EXPECT_EQ(rep_res.size(), 2);
    EXPECT_EQ(rep_res.find("GPU0") != rep_res.end(), true);
    EXPECT_EQ(rep_res.find("VR0") != rep_res.end(), true);

    // std::cout << rep_res;
    EXPECT_EQ(rep_res["GPU0"].layer["data_dump"].size(), 0);
    EXPECT_EQ(rep_res["GPU0"].layer["erot_control"].size(), 0);
    EXPECT_EQ(rep_res["GPU0"].layer["power_rail"].size(),
              jgpu0["power_rail"].size());
    EXPECT_EQ(rep_res["GPU0"].layer["pin_status"].size(),
              jgpu0["pin_status"].size());
    EXPECT_EQ(rep_res["GPU0"].layer["interface_status"].size(),
              jgpu0["interface_status"].size());
    EXPECT_EQ(rep_res["GPU0"].layer["firmware_status"].size(),
              jgpu0["firmware_status"].size());
    EXPECT_EQ(rep_res["GPU0"].layer["protocol_status"].size(),
              jgpu0["protocol_status"].size());

    EXPECT_EQ(rep_res["VR0"].layer["data_dump"].size(), 0);
    EXPECT_EQ(rep_res["VR0"].layer["erot_control"].size(), 0);
    EXPECT_EQ(rep_res["VR0"].layer["power_rail"].size(),
              jvr0["power_rail"].size());
    EXPECT_EQ(rep_res["VR0"].layer["pin_status"].size(),
              jvr0["pin_status"].size());
    EXPECT_EQ(rep_res["VR0"].layer["interface_status"].size(),
              jvr0["interface_status"].size());
    EXPECT_EQ(rep_res["VR0"].layer["firmware_status"].size(),
              jvr0["firmware_status"].size());
    EXPECT_EQ(rep_res["VR0"].layer["protocol_status"].size(),
              jvr0["protocol_status"].size());

    EXPECT_EQ(rep_res["GPU0"].layer.count("data_dump"), 1);
    EXPECT_EQ(rep_res["GPU0"].layer.count("power_rail"), 1);
    EXPECT_EQ(rep_res["GPU0"].layer.count("erot_control"), 1);
    EXPECT_EQ(rep_res["GPU0"].layer.count("pin_status"), 1);
    EXPECT_EQ(rep_res["GPU0"].layer.count("interface_status"), 1);
    EXPECT_EQ(rep_res["GPU0"].layer.count("firmware_status"), 1);
    EXPECT_EQ(rep_res["GPU0"].layer.count("protocol_status"), 1);

    EXPECT_EQ(rep_res["VR0"].layer.count("data_dump"), 1);
    EXPECT_EQ(rep_res["VR0"].layer.count("power_rail"), 1);
    EXPECT_EQ(rep_res["VR0"].layer.count("erot_control"), 1);
    EXPECT_EQ(rep_res["VR0"].layer.count("pin_status"), 1);
    EXPECT_EQ(rep_res["VR0"].layer.count("interface_status"), 1);
    EXPECT_EQ(rep_res["VR0"].layer.count("firmware_status"), 1);
    EXPECT_EQ(rep_res["VR0"].layer.count("protocol_status"), 1);
}

TEST(selftestTest, doPerformEntireTree)
{
    /*  Prepared DAT with multiple unique devices, not associated by test point
        binding.
        Expected: report count equal to DAT unique devices count. */
    nlohmann::json jdat;
    nlohmann::json jDevTemplate;
    nlohmann::json jbaseboard;
    jDevTemplate = json_device_template("dummy");
    jDevTemplate["power_rail"] = {json_testpoint_template()};
    jDevTemplate["erot_control"] = {json_testpoint_template()};
    jDevTemplate["pin_status"] = {json_testpoint_template()};
    jDevTemplate["interface_status"] = {json_testpoint_template()};
    jDevTemplate["firmware_status"] = {json_testpoint_template()};
    jDevTemplate["protocol_status"] = {json_testpoint_template()};

    jbaseboard = json_device_template("Baseboard");
    jbaseboard["association"] += "GPU0";
    jbaseboard["association"] += "GPU1";
    jbaseboard["association"] += "GPU2";
    jbaseboard["association"] += "GPU3";
    jbaseboard["association"] += "NVSwitch0";
    jbaseboard["association"] += "NVSwitch0";

    std::list<std::string> devs = {
        "GPU0",      "GPU1",     "GPU2",     "GPU3", "NVSwitch0",
        "NVSwitch1", "Retimer0", "Retimer1", "VR0",  "VR1"};

    jdat["Baseboard"] = jbaseboard;
    for (auto& dev : devs)
    {
        jdat[dev] = jDevTemplate;
    }

    std::map<std::string, dat_traverse::Device> datMap;
    populateMapMock(datMap, jdat);
    // dat_traverse::Device::printTree(datMap);
    selftest::Selftest selftest("selftestObj", datMap);
    selftest::ReportResult rep_res;

    EXPECT_EQ(selftest.performEntireTree(rep_res), aml::RcCode::succ);
    EXPECT_EQ(rep_res.size(), devs.size() + 1); /* + 1 for baseboard */
    for (auto& dev : devs)
    {
        EXPECT_EQ(rep_res.find(dev) != rep_res.end(), true);
    }
    EXPECT_EQ(rep_res.find("GPU10") != rep_res.end(), false);
}

TEST(selftestTest, doProcess)
{
    /* Given: prepared DAT, prepared event.
       When: called process on prepared event.
       Then: process returns correct value to given input. */
    nlohmann::json jdat;
    nlohmann::json jgpu0;
    nlohmann::json jvr0;
    nlohmann::json jbaseboard;
    jgpu0 = json_device_template("GPU0", "VR0");
    jgpu0["power_rail"] += json_testpoint_template("PGOOD", true, "VR0");
    jgpu0["power_rail"] += json_testpoint_template("PGOOD", false);
    jgpu0["erot_control"] = {json_testpoint_template()};
    jgpu0["pin_status"] = {json_testpoint_template()};
    jgpu0["interface_status"] = {json_testpoint_template()};
    jgpu0["firmware_status"] = {json_testpoint_template()};
    jgpu0["protocol_status"] = {json_testpoint_template()};

    jvr0 = json_device_template("VR0");
    jvr0["power_rail"] = {json_testpoint_template()};
    jvr0["erot_control"] = {json_testpoint_template()};
    jvr0["pin_status"] = {json_testpoint_template()};
    jvr0["interface_status"] = {json_testpoint_template()};
    jvr0["firmware_status"] = {json_testpoint_template()};
    jvr0["protocol_status"] = {json_testpoint_template()};

    jbaseboard = json_device_template("Baseboard");
    jbaseboard["association"] += "GPU0";
    jbaseboard["association"] += "VR0";

    jdat["Baseboard"] = jbaseboard;
    jdat["GPU0"] = jgpu0;
    jdat["VR0"] = jvr0;

    // std::cout << jgpu0.dump(4) << std::endl;
    // std::cout << jvr0.dump(4) << std::endl;
    // std::cout << jbaseboard.dump(4) << std::endl;
    // std::cout << jdat.dump(4) << std::endl;

    std::map<std::string, dat_traverse::Device> datMap;
    populateMapMock(datMap, jdat);
    // dat_traverse::Device::printTree(datMap);
    selftest::Selftest selftest("selftestObj", datMap);
    event_info::EventNode event("test event");
    event.device = "GPU0";

    EXPECT_EQ(selftest.process(event), aml::RcCode::succ);

    /* purposely fail single TP */
    auto tp = datMap.at("VR0").test["power_rail"].testPoints.begin();
    tp->second.expectedValue = "force_test_to_fail";
    EXPECT_EQ(selftest.process(event), aml::RcCode::error);

    event.device = "trash_device";
    aml::RcCode result = aml::RcCode::succ;
    EXPECT_NO_THROW(result = selftest.process(event));
    EXPECT_EQ(result, aml::RcCode::error);
}

TEST(selftestTest, wrongDeviceTestpoint)
{
    /* Device_A: (dummy TP's) + TP pointing to wrong device,
       which is not present in DAT map.
       Expected: no exception is thrown but the result is aml::RcCode::error */
    nlohmann::json jdat;
    nlohmann::json jgpu0;
    nlohmann::json jvr0;
    nlohmann::json jbaseboard;
    jgpu0 = json_device_template("GPU0", "VR0");
    jgpu0["power_rail"] +=
        json_testpoint_template("PGOOD", true, "unknown_device");
    jgpu0["power_rail"] += json_testpoint_template("PGOOD", false);
    jgpu0["erot_control"] = {json_testpoint_template()};
    jgpu0["pin_status"] = {json_testpoint_template()};
    jgpu0["interface_status"] = {json_testpoint_template()};
    jgpu0["firmware_status"] = {json_testpoint_template()};
    jgpu0["protocol_status"] = {json_testpoint_template()};

    jvr0 = json_device_template("VR0");
    jvr0["power_rail"] = {json_testpoint_template()};
    jvr0["erot_control"] = {json_testpoint_template()};
    jvr0["pin_status"] = {json_testpoint_template()};
    jvr0["interface_status"] = {json_testpoint_template()};
    jvr0["firmware_status"] = {json_testpoint_template()};
    jvr0["protocol_status"] = {json_testpoint_template()};

    jbaseboard = json_device_template("Baseboard");
    jbaseboard["association"] += "GPU0";
    jbaseboard["association"] += "VR0";

    jdat["Baseboard"] = jbaseboard;
    jdat["GPU0"] = jgpu0;
    jdat["VR0"] = jvr0;

    // std::cout << jgpu0.dump(4) << std::endl;
    // std::cout << jvr0.dump(4) << std::endl;
    // std::cout << jbaseboard.dump(4) << std::endl;
    // std::cout << jdat.dump(4) << std::endl;

    std::map<std::string, dat_traverse::Device> datMap;
    populateMapMock(datMap, jdat);
    // dat_traverse::Device::printTree(datMap);
    selftest::Selftest selftest("selftestObj", datMap);
    selftest::ReportResult rep_res;
    aml::RcCode result = aml::RcCode::succ;
    EXPECT_NO_THROW(result = selftest.perform(datMap.at("GPU0"), rep_res));
    EXPECT_EQ(result, aml::RcCode::error);
}

TEST(selftestTest, cachesReports)
{
    /* Device_A: (dummy TP's) + multiple TP pointing to Device_C
                multiple TP pointing to Device_B.
       Device_B: (dummy TP's) + multiple TP pointing to Device_C.
       Device_C: (dummy TP's).
       Expected: generated report has only unique devices
       which is 3 in this case (Dev_A, Dev_B, Dev_C). */
    nlohmann::json jdat;
    nlohmann::json jgpu0;
    nlohmann::json jvr0;
    nlohmann::json jhsc0;
    nlohmann::json jbaseboard;
    jgpu0 = json_device_template("GPU0");
    jgpu0["power_rail"] += json_testpoint_template("PGOOD", true, "HSC0");
    jgpu0["power_rail"] += json_testpoint_template("PGOOD", true, "VR0");
    jgpu0["power_rail"] += json_testpoint_template("PGOOD", false);
    jgpu0["erot_control"] = {json_testpoint_template()};
    jgpu0["pin_status"] += json_testpoint_template("PGOOD", true, "HSC0");
    jgpu0["pin_status"] += json_testpoint_template("PGOOD", true, "VR0");
    jgpu0["pin_status"] += json_testpoint_template("PGOOD", false);
    jgpu0["interface_status"] = {json_testpoint_template()};
    jgpu0["firmware_status"] = {json_testpoint_template()};
    jgpu0["protocol_status"] = {json_testpoint_template()};

    jvr0 = json_device_template("VR0");
    jvr0["power_rail"] = {json_testpoint_template("PGOOD", false)};
    jvr0["erot_control"] += json_testpoint_template("PGOOD", true, "HSC0");
    jvr0["erot_control"] += json_testpoint_template("dummy", false);
    jvr0["pin_status"] = {json_testpoint_template()};
    jvr0["interface_status"] = {json_testpoint_template()};
    jvr0["firmware_status"] = {json_testpoint_template()};
    jvr0["protocol_status"] += json_testpoint_template("PGOOD", true, "HSC0");
    jvr0["protocol_status"] += json_testpoint_template("dummy", false);

    jhsc0 = json_device_template("HSC0");
    jhsc0["power_rail"] = {json_testpoint_template()};
    jhsc0["erot_control"] = {json_testpoint_template()};
    jhsc0["pin_status"] = {json_testpoint_template()};
    jhsc0["interface_status"] = {json_testpoint_template()};
    jhsc0["firmware_status"] = {json_testpoint_template()};
    jhsc0["protocol_status"] = {json_testpoint_template()};

    jbaseboard = json_device_template("Baseboard");
    jbaseboard["association"] += "GPU0";
    jbaseboard["association"] += "VR0";
    jbaseboard["association"] += "HSC0";

    jdat["Baseboard"] = jbaseboard;
    jdat["GPU0"] = jgpu0;
    jdat["VR0"] = jvr0;
    jdat["HSC0"] = jhsc0;

    // std::cout << jgpu0.dump(4) << std::endl;
    // std::cout << jvr0.dump(4) << std::endl;
    // std::cout << jhsc0.dump(4) << std::endl;
    // std::cout << jbaseboard.dump(4) << std::endl;
    // std::cout << jdat.dump(4) << std::endl;

    std::map<std::string, dat_traverse::Device> datMap;
    populateMapMock(datMap, jdat);
    // dat_traverse::Device::printTree(datMap);
    selftest::Selftest selftest("selftestObj", datMap);
    selftest::ReportResult rep_res;
    EXPECT_EQ(selftest.perform(datMap.at("GPU0"), rep_res), aml::RcCode::succ);
    EXPECT_EQ(rep_res.size(), 3);
    EXPECT_EQ(rep_res.find("GPU0") != rep_res.end(), true);
    EXPECT_EQ(rep_res.find("VR0") != rep_res.end(), true);
    EXPECT_EQ(rep_res.find("HSC0") != rep_res.end(), true);
    for (const auto& layer : layers)
    {
        EXPECT_EQ(rep_res["GPU0"].layer[layer].size(), jgpu0[layer].size());
        EXPECT_EQ(rep_res["VR0"].layer[layer].size(), jvr0[layer].size());
        EXPECT_EQ(rep_res["HSC0"].layer[layer].size(), jhsc0[layer].size());
    }
    // std::cout << rep_res;
}

TEST(selftestTest, highlyRecursedDevices)
{
    /* Device_A: (dummy TP's) + TP pointing to Device_B.
       Device_B: (dummy TP's) + TP pointing to Device_C.
       Device_C: (dummy TP's) + TP pointing to Device_D.
       Device_D: (dummy TP's) + TP pointing to Device_E.
       Device_E: (dummy TP's).
       Expected: report of 5 devices (which were linked
       to each other in DAT map through test points). */

    nlohmann::json jdat;
    nlohmann::json jgpu0;
    nlohmann::json jvr0;
    nlohmann::json jhsc0;
    nlohmann::json jnvlink0;
    nlohmann::json jerot0;
    nlohmann::json jbaseboard;
    jgpu0 = json_device_template("GPU0");
    jgpu0["power_rail"] += json_testpoint_template("PGOOD", true, "VR0");
    jgpu0["power_rail"] += json_testpoint_template("PGOOD", false);
    jgpu0["erot_control"] = {json_testpoint_template()};
    jgpu0["pin_status"] = {json_testpoint_template()};
    jgpu0["interface_status"] = {json_testpoint_template()};
    jgpu0["firmware_status"] = {json_testpoint_template()};
    jgpu0["protocol_status"] = {json_testpoint_template()};

    jvr0 = json_device_template("VR0");
    jvr0["power_rail"] += json_testpoint_template("PGOOD", true, "HSC0");
    jvr0["power_rail"] += json_testpoint_template("PGOOD", false);
    jvr0["erot_control"] = {json_testpoint_template()};
    jvr0["pin_status"] = {json_testpoint_template()};
    jvr0["interface_status"] = {json_testpoint_template()};
    jvr0["firmware_status"] = {json_testpoint_template()};
    jvr0["protocol_status"] = {json_testpoint_template()};

    jhsc0 = json_device_template("HSC0");
    jhsc0["power_rail"] += json_testpoint_template("PGOOD", true, "NVLINK0");
    jhsc0["power_rail"] += json_testpoint_template("PGOOD", false);
    jhsc0["erot_control"] = {json_testpoint_template()};
    jhsc0["pin_status"] = {json_testpoint_template()};
    jhsc0["interface_status"] = {json_testpoint_template()};
    jhsc0["firmware_status"] = {json_testpoint_template()};
    jhsc0["protocol_status"] = {json_testpoint_template()};

    jnvlink0 = json_device_template("NVLINK0");
    jnvlink0["power_rail"] += json_testpoint_template("PGOOD", true, "EROT0");
    jnvlink0["power_rail"] += json_testpoint_template("PGOOD", false);
    jnvlink0["erot_control"] = {json_testpoint_template()};
    jnvlink0["pin_status"] = {json_testpoint_template()};
    jnvlink0["interface_status"] = {json_testpoint_template()};
    jnvlink0["firmware_status"] = {json_testpoint_template()};
    jnvlink0["protocol_status"] = {json_testpoint_template()};

    jerot0 = json_device_template("EROT0");
    jerot0["power_rail"] = {json_testpoint_template()};
    jerot0["erot_control"] = {json_testpoint_template()};
    jerot0["pin_status"] = {json_testpoint_template()};
    jerot0["interface_status"] = {json_testpoint_template()};
    jerot0["firmware_status"] = {json_testpoint_template()};
    jerot0["protocol_status"] = {json_testpoint_template()};

    jbaseboard = json_device_template("Baseboard");
    jbaseboard["association"] += "GPU0";
    jbaseboard["association"] += "VR0";
    jbaseboard["association"] += "HSC0";
    jbaseboard["association"] += "NVLINK0";
    jbaseboard["association"] += "EROT0";

    jdat["Baseboard"] = jbaseboard;
    jdat["GPU0"] = jgpu0;
    jdat["VR0"] = jvr0;
    jdat["HSC0"] = jhsc0;
    jdat["NVLINK0"] = jnvlink0;
    jdat["EROT0"] = jerot0;

    // std::cout << jgpu0.dump(4) << std::endl;
    // std::cout << jvr0.dump(4) << std::endl;
    // std::cout << jhsc0.dump(4) << std::endl;
    // std::cout << jbaseboard.dump(4) << std::endl;
    // std::cout << jdat.dump(4) << std::endl;

    std::map<std::string, dat_traverse::Device> datMap;
    populateMapMock(datMap, jdat);
    // dat_traverse::Device::printTree(datMap);
    selftest::Selftest selftest("selftestObj", datMap);
    selftest::ReportResult rep_res;
    EXPECT_EQ(selftest.perform(datMap.at("GPU0"), rep_res), aml::RcCode::succ);
    EXPECT_EQ(rep_res.size(), 5);
    EXPECT_EQ(rep_res.find("GPU0") != rep_res.end(), true);
    EXPECT_EQ(rep_res.find("VR0") != rep_res.end(), true);
    EXPECT_EQ(rep_res.find("HSC0") != rep_res.end(), true);
    EXPECT_EQ(rep_res.find("NVLINK0") != rep_res.end(), true);
    EXPECT_EQ(rep_res.find("EROT0") != rep_res.end(), true);
    for (const auto& layer : layers)
    {
        EXPECT_EQ(rep_res["GPU0"].layer[layer].size(), jgpu0[layer].size());
        EXPECT_EQ(rep_res["VR0"].layer[layer].size(), jvr0[layer].size());
        EXPECT_EQ(rep_res["HSC0"].layer[layer].size(), jhsc0[layer].size());
        EXPECT_EQ(rep_res["NVLINK0"].layer[layer].size(),
                  jnvlink0[layer].size());
        EXPECT_EQ(rep_res["EROT0"].layer[layer].size(), jerot0[layer].size());
    }
    // std::cout << rep_res;
}

TEST(selftestTest, immuneToDeviceTestPointCircle)
{
    /* Device_A: (dummy TP's) + TP_2 of Device_A points to Device_B.
       Device_B: TP_1 is dummy, TP_2 of Device_B points to Device_A
       forming a circle.
       Expected: selftest finishes (doesn't hang because of device loop)
       and generates report of 2 unique devices. */
    nlohmann::json jdat;
    nlohmann::json jgpu0;
    nlohmann::json jvr0;
    nlohmann::json jbaseboard;
    jgpu0 = json_device_template("GPU0");
    jgpu0["power_rail"] += json_testpoint_template("PGOOD", true, "VR0");
    jgpu0["power_rail"] += json_testpoint_template("PGOOD", false);
    jgpu0["erot_control"] = {json_testpoint_template()};
    jgpu0["pin_status"] = {json_testpoint_template()};
    jgpu0["interface_status"] = {json_testpoint_template()};
    jgpu0["firmware_status"] = {json_testpoint_template()};
    jgpu0["protocol_status"] = {json_testpoint_template()};

    jvr0 = json_device_template("VR0");
    jvr0["power_rail"] += json_testpoint_template("PGOOD", true, "GPU0");
    jvr0["power_rail"] += json_testpoint_template("PGOOD", false);
    jvr0["erot_control"] = {json_testpoint_template()};
    jvr0["pin_status"] = {json_testpoint_template()};
    jvr0["interface_status"] = {json_testpoint_template()};
    jvr0["firmware_status"] = {json_testpoint_template()};
    jvr0["protocol_status"] = {json_testpoint_template()};

    jbaseboard = json_device_template("Baseboard");
    jbaseboard["association"] += "GPU0";
    jbaseboard["association"] += "VR0";

    jdat["Baseboard"] = jbaseboard;
    jdat["GPU0"] = jgpu0;
    jdat["VR0"] = jvr0;

    // std::cout << jgpu0.dump(4) << std::endl;
    // std::cout << jvr0.dump(4) << std::endl;
    // std::cout << jbaseboard.dump(4) << std::endl;
    // std::cout << jdat.dump(4) << std::endl;

    std::map<std::string, dat_traverse::Device> datMap;
    populateMapMock(datMap, jdat);
    // dat_traverse::Device::printTree(datMap);
    selftest::Selftest selftest("selftestObj", datMap);
    selftest::ReportResult rep_res;
    EXPECT_EQ(selftest.perform(datMap.at("GPU0"), rep_res), aml::RcCode::succ);
    EXPECT_EQ(rep_res.size(), 2);
    EXPECT_EQ(rep_res.find("GPU0") != rep_res.end(), true);
    EXPECT_EQ(rep_res.find("VR0") != rep_res.end(), true);
    for (const auto& layer : layers)
    {
        EXPECT_EQ(rep_res["GPU0"].layer[layer].size(), jgpu0[layer].size());
        EXPECT_EQ(rep_res["VR0"].layer[layer].size(), jvr0[layer].size());
    }
    // std::cout << rep_res;
}

/* ========================== report tests ================================= */

/* @note use only with generateReport UT because of hardcoded values */
static inline void generateReportSubtestLayers(nlohmann::json j,
                                               bool expected_result)
{
    std::list<std::string> reportLayers = {
        "power-rail-status", "erot-control-status", "pin-status",
        "interface-status",  "protocol-status",     "firmware-status"};
    std::string expResult = expected_result ? "Pass" : "Fail";
    std::string expVal = expected_result ? "OK" : "123";

    for (auto& layer : reportLayers)
    {
        EXPECT_EQ(j[layer]["result"], expResult);

        int tpSize = j[layer]["test-points"].size();
        auto& jTp = j[layer]["test-points"];
        for (auto i = 0; i < tpSize; i++)
        {
            EXPECT_EQ(jTp[i]["name"], std::string("sampleTp"));
            EXPECT_EQ(jTp[i]["value"], expVal);
            EXPECT_EQ(jTp[i]["value-expected"], "OK");
            EXPECT_EQ(jTp[i]["result"], expResult);
        }
    }
}

TEST(selftestReportTest, generateReport)
{
    /*  Given synthetic ReportResult with:
        Dev_A : 6 layers : each with failed TP
        Dev_B : 6 layers : each layer with passed TP
        Total 12TP in which 6 failed and 6 passed.
        Call generate report.
        Compare with expected results */

    /* Given */
    selftest::ReportResult reportResult;
    struct selftest::TestPointResult dummyTp = {.targetName = "sampleTp",
                                                .valRead = "123",
                                                .valExpected = "OK",
                                                .result = false};

    std::vector<struct selftest::TestPointResult> tpVec;
    tpVec.insert(tpVec.end(), dummyTp);

    struct selftest::DeviceResult dummyDevice;
    dummyDevice.layer["power_rail"] = tpVec;
    dummyDevice.layer["erot_control"] = tpVec;
    dummyDevice.layer["pin_status"] = tpVec;
    dummyDevice.layer["interface_status"] = tpVec;
    dummyDevice.layer["firmware_status"] = tpVec;
    dummyDevice.layer["protocol_status"] = tpVec;

    reportResult["GPU0"] = dummyDevice;
    reportResult["HSC0"] = dummyDevice;

    /* overwrite failed TP to passed for second device */
    dummyTp.valRead = "OK";
    dummyTp.result = true;
    reportResult["HSC0"].layer["power_rail"][0] = dummyTp;
    reportResult["HSC0"].layer["erot_control"][0] = dummyTp;
    reportResult["HSC0"].layer["pin_status"][0] = dummyTp;
    reportResult["HSC0"].layer["interface_status"][0] = dummyTp;
    reportResult["HSC0"].layer["firmware_status"][0] = dummyTp;
    reportResult["HSC0"].layer["protocol_status"][0] = dummyTp;
    // std::cout << reportResult;

    /* When */
    selftest::Report reportGenerator;
    EXPECT_EQ(true, reportGenerator.generateReport(reportResult));

    /* Then */
    auto j = reportGenerator.getReport();
    EXPECT_EQ(j["header"]["name"], "Self test report");
    EXPECT_EQ(j["header"]["summary"]["test-case-failed"], 6);
    EXPECT_EQ(j["header"]["summary"]["test-case-total"], 12);
    EXPECT_EQ(j["header"]["timestamp"].type(), nlohmann::json::value_t::string);
    EXPECT_EQ(j["header"]["version"], "1.0");

    EXPECT_EQ(j["tests"][0]["device-name"], "GPU0");
    EXPECT_EQ(j["tests"][0]["timestamp"].type(),
              nlohmann::json::value_t::string);
    EXPECT_EQ(j["tests"][0]["firmware-version"], "<todo>");
    generateReportSubtestLayers(j["tests"][0], false);

    EXPECT_EQ(j["tests"][1]["device-name"], "HSC0");
    EXPECT_EQ(j["tests"][1]["timestamp"].type(),
              nlohmann::json::value_t::string);
    EXPECT_EQ(j["tests"][1]["firmware-version"], "<todo>");
    generateReportSubtestLayers(j["tests"][1], true);

    // std::cout << reportGenerator;
}

TEST(selftestReportTest, fieldOrder)
{
    /* Generate dummy report, expect that:
        1. header key is first and test key is second
        2. inside every device key: layer keys are preceeding datadump key
        3. TP result without expected value is simplified to name and read
            fields only */

    /* Given */
    selftest::ReportResult reportResult;
    struct selftest::TestPointResult dummyTpExp = {.targetName = "expValTp",
                                                   .valRead = "dummy read",
                                                   .valExpected = "OK",
                                                   .result = false};

    struct selftest::TestPointResult dummyTpNoExp = {.targetName = "noExpValTp",
                                                     .valRead = "dummy read",
                                                     .valExpected = "",
                                                     .result = true};

    std::vector<struct selftest::TestPointResult> tpVec;
    tpVec.insert(tpVec.end(), dummyTpExp);
    tpVec.insert(tpVec.end(), dummyTpNoExp);

    struct selftest::DeviceResult dummyDevice;
    dummyDevice.layer["power_rail"] = tpVec;
    dummyDevice.layer["erot_control"] = tpVec;
    dummyDevice.layer["pin_status"] = tpVec;
    dummyDevice.layer["interface_status"] = tpVec;
    dummyDevice.layer["firmware_status"] = tpVec;
    dummyDevice.layer["protocol_status"] = tpVec;
    dummyDevice.layer["data_dump"] = tpVec;

    reportResult["GPU0"] = dummyDevice;

    /* When */
    selftest::Report reportGenerator;
    EXPECT_EQ(true, reportGenerator.generateReport(reportResult));

    /* Then */
    auto j = reportGenerator.getReport();
    auto idx = 0;

    /* check 1. requirement */
    const std::vector<std::string> mainKeyOrderLut = {"header", "tests"};
    idx = 0;
    for (auto& el : j.items())
    {
        EXPECT_EQ(el.key(), mainKeyOrderLut.at(idx));
        idx++;
    }

    /* check 2. requirement */
    const std::vector<std::string> deviceKeysOrderLut = {
        "device-name",     "firmware-version",
        "timestamp",       "erot-control-status",
        "firmware-status", "interface-status",
        "pin-status",      "power-rail-status",
        "protocol-status", "data-dump"};
    idx = 0;

    for (auto& el : j.at("tests").at(0).items())
    {
        EXPECT_EQ(el.key(), deviceKeysOrderLut.at(idx));
        idx++;
    }

    /* check 3. requirement */
    auto TPs = j["tests"][0]["data-dump"]["test-points"];
    EXPECT_EQ(TPs[0].count("name"), 1);
    EXPECT_EQ(TPs[0]["name"], dummyTpExp.targetName);
    EXPECT_EQ(TPs[0].count("value"), 1);
    EXPECT_EQ(TPs[0]["value"], dummyTpExp.valRead);
    EXPECT_EQ(TPs[0].count("value-expected"), 1);
    EXPECT_EQ(TPs[0]["value-expected"], dummyTpExp.valExpected);
    EXPECT_EQ(TPs[0].count("result"), 1);
    EXPECT_EQ(TPs[0]["result"], dummyTpExp.result ? "Pass" : "Fail");

    EXPECT_EQ(TPs[1].count("name"), 1);
    EXPECT_EQ(TPs[1]["name"], dummyTpNoExp.targetName);
    EXPECT_EQ(TPs[1].count("value"), 1);
    EXPECT_EQ(TPs[1]["value"], dummyTpNoExp.valRead);
    EXPECT_EQ(TPs[1].count("value-expected"), 0);
    EXPECT_EQ(TPs[1].count("result"), 0);
}

/* ===================== rootCauseTracer tests ============================ */

TEST(rootCauseTraceTest, test1)
{
    /*  Associations:
        GPU0 -> HSC0, GPU0-ERot, Retimer0
        HSC0 -> none
        GPU0-ERot -> none
        Retimer0 -> HSC8
        HSC8 -> none
        First all TP pass, then break most nested HSC8 TP, then break less
        nested Retimer0 TP. Observe root cause change and health change.
        The less nested device fails, the smaller event report content is, as it
        stops on first association failed. */
    nlohmann::json jTemplateDevice;
    jTemplateDevice = json_device_template("template");
    jTemplateDevice["power_rail"] = {json_testpoint_template()};
    jTemplateDevice["erot_control"] = {json_testpoint_template()};
    jTemplateDevice["pin_status"] = {json_testpoint_template()};
    jTemplateDevice["interface_status"] = {json_testpoint_template()};
    jTemplateDevice["firmware_status"] = {json_testpoint_template()};
    jTemplateDevice["protocol_status"] = {json_testpoint_template()};

    nlohmann::json jgpu0 = jTemplateDevice;
    jgpu0["name"] = "GPU0";
    jgpu0["association"] = {"HSC0", "GPU0-ERoT", "Retimer0"};
    jgpu0["power_rail"] = {json_testpoint_template("nestedHSC", true, "HSC0")};
    jgpu0["erot_control"] = {
        json_testpoint_template("nestedEROT", true, "GPU0-ERoT")};
    jgpu0["interface_status"] = {
        json_testpoint_template("nestRETIMER", true, "Retimer0")};
    dat_traverse::Device gpu0("GPU0", jgpu0);

    nlohmann::json jhsc0 = jTemplateDevice;
    jhsc0["name"] = "HSC0";
    jhsc0["association"] = nlohmann::json::array();
    dat_traverse::Device hsc0("HSC0", jhsc0);

    nlohmann::json jgpu0erot = jTemplateDevice;
    jgpu0erot["name"] = "GPU0-ERoT";
    jgpu0erot["association"] = nlohmann::json::array();
    dat_traverse::Device gpu0erot("GPU0-ERoT", jgpu0erot);

    nlohmann::json jretimer0 = jTemplateDevice;
    jretimer0["name"] = "Retimer0";
    jretimer0["association"] = {"HSC8"};
    jretimer0["power_rail"] = {
        json_testpoint_template("nestedHSC", true, "HSC8")};
    dat_traverse::Device retimer0("Retimer0", jretimer0);

    nlohmann::json jhsc8 = jTemplateDevice;
    jhsc8["name"] = "HSC8";
    jhsc8["association"] = nlohmann::json::array();
    dat_traverse::Device hsc8("HSC8", jhsc8);

    std::map<std::string, dat_traverse::Device> dat;
    dat.insert(std::pair<std::string, dat_traverse::Device>(gpu0.name, gpu0));
    dat.insert(std::pair<std::string, dat_traverse::Device>(hsc0.name, hsc0));
    dat.insert(
        std::pair<std::string, dat_traverse::Device>(gpu0erot.name, gpu0erot));
    dat.insert(
        std::pair<std::string, dat_traverse::Device>(retimer0.name, retimer0));
    dat.insert(std::pair<std::string, dat_traverse::Device>(hsc8.name, hsc8));

    event_handler::RootCauseTracer rootCauseTracer("testTracer", dat);
    event_info::EventNode event("test event");
    event.device = "GPU0";

    EXPECT_EQ(rootCauseTracer.process(event), aml::RcCode::succ);
    EXPECT_EQ(event.selftestReport["header"]["summary"]["test-case-failed"], 0);
    EXPECT_EQ(event.selftestReport["tests"].size(), 5);
    EXPECT_EQ(dat.at("GPU0").healthStatus.healthRollup, "OK");
    EXPECT_EQ(dat.at("GPU0").healthStatus.health, "OK");
    EXPECT_EQ(dat.at("GPU0").healthStatus.triState, "Active");
    EXPECT_EQ(dat.at("GPU0").healthStatus.originOfCondition, "");

    /* fail least nested GPU0 */
    dat.at("GPU0").test["pin_status"].testPoints.begin()->second.expectedValue =
        "force_test_to_fail";

    EXPECT_EQ(rootCauseTracer.process(event), aml::RcCode::succ);
    EXPECT_EQ(event.selftestReport["header"]["summary"]["test-case-failed"], 1);
    EXPECT_EQ(event.selftestReport["tests"].size(), 5);
    EXPECT_EQ(dat.at("GPU0").healthStatus.originOfCondition, "GPU0");

    /* fail nested Retimer0, check if GPU0 gets OOC set as Retimer0 */
    dat.at("Retimer0")
        .test["pin_status"]
        .testPoints.begin()
        ->second.expectedValue = "force_test_to_fail";

    EXPECT_EQ(rootCauseTracer.process(event), aml::RcCode::succ);
    /*  test case failed count is a sum of purposely failed GPU0 above +
        + GPU0 nested device TP (retimer) also failed + retimer's single
        failed TP, 3 in total */
    EXPECT_EQ(event.selftestReport["header"]["summary"]["test-case-failed"], 3);
    EXPECT_EQ(event.selftestReport["tests"].size(), 5);
    EXPECT_EQ(dat.at("GPU0").healthStatus.originOfCondition, "Retimer0");

    /* fail nested HSC8, check if GPU0 gets OOC set as HSC8 */
    dat.at("HSC8").test["pin_status"].testPoints.begin()->second.expectedValue =
        "force_test_to_fail";

    EXPECT_EQ(rootCauseTracer.process(event), aml::RcCode::succ);
    EXPECT_EQ(event.selftestReport["header"]["summary"]["test-case-failed"], 5);
    EXPECT_EQ(event.selftestReport["tests"].size(), 5);
    EXPECT_EQ(dat.at("GPU0").healthStatus.healthRollup, "Critical");
    EXPECT_EQ(dat.at("GPU0").healthStatus.health, "OK");
    EXPECT_EQ(dat.at("GPU0").healthStatus.triState, "Error");
    EXPECT_EQ(dat.at("GPU0").healthStatus.originOfCondition, "HSC8");

    aml::RcCode result = aml::RcCode::succ;
    event.device = "trash_device";
    EXPECT_NO_THROW(result = rootCauseTracer.process(event));
    EXPECT_EQ(result, aml::RcCode::error);
}
