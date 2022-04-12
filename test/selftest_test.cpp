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
    j["expected_value"] = "123";

    if (is_accessor_device)
    {
        j["accessor"]["type"] = "DEVICE";
        j["accessor"]["device_name"] = opt_dev_name;
        return j;
    }

    j["accessor"]["type"] = "DBUS";
    j["accessor"]["object"] =
        "/xyz/openbmc_project/inventory/system/chassis/GPU[0-7]";
    j["accessor"]["interface"] =
        "xyz.openbmc_project.Inventory.Decorator.Dimension";
    j["accessor"]["propperty"] = "Depth";
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
        and Dev_B report */
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
    EXPECT_ANY_THROW(selftest.process(event));
}

TEST(selftestTest, wrongDeviceTestpoint)
{
    /* Device_A: (dummy TP's) + TP pointing to wrong device,
       which is not present in DAT map.
       Expected: exception is thrown. */
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
    EXPECT_ANY_THROW(selftest.perform(datMap.at("GPU0"), rep_res));
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
    reportGenerator.generateReport(reportResult);

    /* Then */
    nlohmann::json j = reportGenerator.getReport();
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
