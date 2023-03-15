#include "aml.hpp"
#include "aml_main.hpp"
#include "dat_traverse.hpp"
#include "data_accessor.hpp"
#include "event_detection.hpp"
#include "event_info.hpp"
#include "message_composer.hpp"
#include "tests_common_defs.hpp"
#include "util.hpp"

#include "gmock/gmock.h"

TEST(JsonSchemaTest, Default)
{
    using namespace json_schema;
    auto ev = event_GPU_VRFailure();
    auto eventSchema = aml::eventInfoJsonSchema();
    json_proc::ProblemsCollector problemsCollector;
    eventSchema->check(ev, problemsCollector);
    for (const auto& problem : problemsCollector)
    {
        std::cout << json_proc::to_string(problem) << std::endl;
    }
}

TEST(EventTest, LoadJson)
{
    nlohmann::json j;
    j["event"] = "Event0";
    j["device_type"] = "GPU";
    j["sub_type"] = "";
    j["severity"] = "Critical";
    j["resolution"] = "Contact NVIDIA Support";
    j["redfish"]["message_id"] = "ResourceEvent.1.0.ResourceErrorsDetected";
    j["redfish"]["message_args"]["patterns"] = {"p1", "p2"};
    j["redfish"]["message_args"]["parameters"] = nlohmann::json::array();
    j["telemetries"] = {"t0", "t1"};
    j["trigger_count"] = 0;
    j["event_trigger"] = "trigger";
    j["action"] = "do something";
    j["event_counter_reset"]["type"] = "type";
    j["event_counter_reset"]["metadata"] = "metadata";
    j["accessor"]["metadata"] = "metadata";
    j["accessor"]["type"] = "DBUS";
    j["value_as_count"] = false;
    event_info::EventNode event("test event");
    event.loadFrom(j);
    EXPECT_EQ(event.event, "Event0");
    EXPECT_EQ(event.getMessageId(), "ResourceEvent.1.0.ResourceErrorsDetected");
    EXPECT_EQ(event.counterReset["metadata"], "metadata");
    EXPECT_EQ(event.getSeverity(), "Critical");
}

TEST(MsgCompTest, MakeCall)
{
    nlohmann::json j;
    j["event"] = "Event0";
    j["device_type"] = "GPU";
    j["sub_type"] = "";
    j["severity"] = "Critical";
    j["resolution"] = "Contact NVIDIA Support";
    j["redfish"]["message_id"] = "ResourceEvent.1.0.ResourceErrorsDetected";
    j["redfish"]["message_args"]["patterns"] = {"p1", "p2"};
    j["redfish"]["message_args"]["parameters"] = nlohmann::json::array();
    j["telemetries"] = {"t0", "t1"};
    j["trigger_count"] = 0;
    j["event_trigger"] = "trigger";
    j["action"] = "do something";
    j["event_counter_reset"]["type"] = "type";
    j["event_counter_reset"]["metadata"] = "metadata";
    j["accessor"]["metadata"] = "metadata";
    j["accessor"]["type"] = "DBUS";
    j["value_as_count"] = false;
    event_info::EventNode event("test event");
    event.loadFrom(j);
    std::map<std::string, dat_traverse::Device> dat;
    message_composer::MessageComposer mc(dat, "Test Msg Composer");
    EXPECT_EQ(mc.getName(), "Test Msg Composer");
}

TEST(DevNameTest, MakeCall)
{
    auto pattern = "/xyz/openbmc_project/inventory/system/chassis/GPU[1-49]";
    auto objPath = "/xyz/openbmc_project/inventory/system/chassis/GPU12";
    auto devType = "GPU[1-49]";
    auto name = util::determineDeviceName(pattern, objPath, devType);
    EXPECT_EQ(name, "GPU12");
}

TEST(EventTelemtries, MakeCall)
{
    nlohmann::json j;
    j["event"] = "Event0";
    j["device_type"] = "GPU";
    j["sub_type"] = "";
    j["severity"] = "Critical";
    j["resolution"] = "Contact NVIDIA Support";
    j["redfish"]["message_id"] = "ResourceEvent.1.0.ResourceErrorsDetected";
    j["redfish"]["message_args"]["patterns"] = {"p1", "p2"};
    j["redfish"]["message_args"]["parameters"] = nlohmann::json::array();
    j["telemetries"] = {
        {{"name", "temperature"},
         {"type", "DBUS"},
         {"object", "xyz.openbmc_project.Inventory.Decorator.Dimension"},
         {"interface", "Depth"}},
        {{"name", "power"},
         {"type", "DBUS"},
         {"object", "xyz.openbmc_project.Inventory.Decorator.Dimension"},
         {"interface", "Depth"}}};
    j["trigger_count"] = 0;
    j["event_trigger"] = "trigger";
    j["action"] = "do something";
    j["event_counter_reset"]["type"] = "type";
    j["event_counter_reset"]["metadata"] = "metadata";
    j["accessor"]["metadata"] = "metadata";
    j["accessor"]["type"] = "DBUS";
    j["value_as_count"] = false;
    event_info::EventNode event("test event");
    event.loadFrom(j);

    auto telemetries =
        message_composer::MessageComposer::collectDiagData(event);

    /*  deserialize object, check content;
        selftest report is expected to be empty in this case as there were none
        performed. */
    nlohmann::json jCollected = nlohmann::json::parse(telemetries);
    EXPECT_EQ(jCollected.size(), 5);
    EXPECT_EQ(jCollected["power"], data_accessor::readFailedReturn);
    EXPECT_EQ(jCollected["temperature"], data_accessor::readFailedReturn);
    EXPECT_EQ(jCollected["selftest"].size(), 0);
}

TEST(EventSeverityLevel, Critical)
{
    auto level = message_composer::MessageComposer::makeSeverity("Critical");
    EXPECT_EQ(level, "xyz.openbmc_project.Logging.Entry.Level.Critical");
}

TEST(EventSeverityLevel, OK)
{
    auto level = message_composer::MessageComposer::makeSeverity("OK");
    EXPECT_EQ(level, "xyz.openbmc_project.Logging.Entry.Level.Informational");
}

TEST(EventSeverityLevel, OkLowerK)
{
    auto level = message_composer::MessageComposer::makeSeverity("Ok");
    EXPECT_EQ(level, "xyz.openbmc_project.Logging.Entry.Level.Informational");
}

TEST(EventSeverityLevel, Warning)
{
    auto level = message_composer::MessageComposer::makeSeverity("Warning");
    EXPECT_EQ(level, "xyz.openbmc_project.Logging.Entry.Level.Warning");
}

TEST(EventSeverityLevel, WarningLowerW)
{
    auto level = message_composer::MessageComposer::makeSeverity("warning");
    EXPECT_EQ(level, "xyz.openbmc_project.Logging.Entry.Level.Warning");
}

TEST(EventInfoTest, EventNode_setDeviceTypes)
{
    event_info::EventNode en;
    EXPECT_NO_THROW(
        en.setDeviceTypes(std::vector<std::string>({"GPU_SXM_[1-8]"})));
    EXPECT_NO_THROW(en.setDeviceTypes(nlohmann::json("GPU_SXM_[1-8]")));
    EXPECT_NO_THROW(en.setDeviceTypes(
        nlohmann::json::array({"GPU_SXM_[1-8]", "NVLink_[0-39]"})));
    EXPECT_ANY_THROW(en.setDeviceTypes(std::vector<std::string>()));
    EXPECT_ANY_THROW(en.setDeviceTypes(nlohmann::json::object()));
    EXPECT_ANY_THROW(en.setDeviceTypes(nlohmann::json::array()));
}

TEST(EventInfoTest, EventNode_getStringifiedDeviceType)
{
    event_info::EventNode en;
    en.setDeviceTypes(
        nlohmann::json::array({"GPU_SXM_[1-8]", "NVLink_[0-39]"}));
    EXPECT_EQ(en.getStringifiedDeviceType(), "GPU_SXM_[1-8]/NVLink_[0-39]");

    en.setDeviceTypes(nlohmann::json::array({"GPU_SXM_[1-8]"}));
    EXPECT_EQ(en.getStringifiedDeviceType(), "GPU_SXM_[1-8]");
}

// "Baseboard_0"
// "FPGA_0"
// "GPU_SXM_[0|1-8]/NVLink_[1|0-17]"
// "GPU_SXM_[1-8]"
// "HMC_0"
// "HSC_[0-9]"
// "NVSwitch_[0|0-3]/NVLink_[1|0-39]"
// "NVSwitch_[0-3]"
// "NVSwitch_[0-3]/NVLink_[0-39]"
// "PCIeRetimer_[0-7]"
// "PCIeSwitch_0"

TEST(EventInfoTest, EventNode_deviceType)
{
    {
        event_info::EventNode en;
        en.setDeviceTypes(nlohmann::json("Baseboard_0"));
        EXPECT_EQ(en.getStringifiedDeviceType(), "Baseboard_0");
        EXPECT_EQ(en.getMainDeviceType(), "Baseboard_0");
    }
    {
        event_info::EventNode en;
        en.setDeviceTypes(nlohmann::json("FPGA_0"));
        EXPECT_EQ(en.getStringifiedDeviceType(), "FPGA_0");
        EXPECT_EQ(en.getMainDeviceType(), "FPGA_0");
    }
    {
        event_info::EventNode en;
        en.setDeviceTypes(nlohmann::json("GPU_SXM_[0|1-8]/NVLink_[1|0-17]"));
        EXPECT_EQ(en.getStringifiedDeviceType(),
                  "GPU_SXM_[0|1-8]/NVLink_[1|0-17]");
        EXPECT_EQ(en.getMainDeviceType(), "GPU_SXM_[0|1-8]");
    }
    {
        event_info::EventNode en;
        en.setDeviceTypes(nlohmann::json("GPU_SXM_[1-8]"));
        EXPECT_EQ(en.getStringifiedDeviceType(), "GPU_SXM_[1-8]");
        EXPECT_EQ(en.getMainDeviceType(), "GPU_SXM_[1-8]");
    }
    {
        event_info::EventNode en;
        en.setDeviceTypes(nlohmann::json("HMC_0"));
        EXPECT_EQ(en.getStringifiedDeviceType(), "HMC_0");
        EXPECT_EQ(en.getMainDeviceType(), "HMC_0");
    }
    {
        event_info::EventNode en;
        en.setDeviceTypes(nlohmann::json("HSC_[0-9]"));
        EXPECT_EQ(en.getStringifiedDeviceType(), "HSC_[0-9]");
        EXPECT_EQ(en.getMainDeviceType(), "HSC_[0-9]");
    }
    {
        event_info::EventNode en;
        en.setDeviceTypes(nlohmann::json("NVSwitch_[0|0-3]/NVLink_[1|0-39]"));
        EXPECT_EQ(en.getStringifiedDeviceType(),
                  "NVSwitch_[0|0-3]/NVLink_[1|0-39]");
        EXPECT_EQ(en.getMainDeviceType(), "NVSwitch_[0|0-3]");
    }
    {
        event_info::EventNode en;
        en.setDeviceTypes(nlohmann::json("NVSwitch_[0-3]"));
        EXPECT_EQ(en.getStringifiedDeviceType(), "NVSwitch_[0-3]");
        EXPECT_EQ(en.getMainDeviceType(), "NVSwitch_[0-3]");
    }
    {
        event_info::EventNode en;
        en.setDeviceTypes(nlohmann::json("NVSwitch_[0-3]/NVLink_[0-39]"));
        EXPECT_EQ(en.getStringifiedDeviceType(),
                  "NVSwitch_[0-3]/NVLink_[0-39]");
        EXPECT_EQ(en.getMainDeviceType(), "NVSwitch_[0-3]");
    }
    {
        event_info::EventNode en;
        en.setDeviceTypes(nlohmann::json("PCIeRetimer_[0-7]"));
        EXPECT_EQ(en.getStringifiedDeviceType(), "PCIeRetimer_[0-7]");
        EXPECT_EQ(en.getMainDeviceType(), "PCIeRetimer_[0-7]");
    }
    {
        event_info::EventNode en;
        en.setDeviceTypes(nlohmann::json("PCIeSwitch_0"));
        EXPECT_EQ(en.getStringifiedDeviceType(), "PCIeSwitch_0");
        EXPECT_EQ(en.getMainDeviceType(), "PCIeSwitch_0");
    }
}

TEST(EventInfoTest, EventCategory_constructor)
{
    for (const std::string& elem : event_info::EventCategory::valuesAllowed)
    {
        EXPECT_NO_THROW({ event_info::EventCategory obj(elem); });
    }
    EXPECT_ANY_THROW(event_info::EventCategory("junk"));
    EXPECT_ANY_THROW(event_info::EventCategory(""));
}

TEST(EventInfoTest, EventCategory_get)
{
    for (const std::string& elem : event_info::EventCategory::valuesAllowed)
    {
        EXPECT_EQ(event_info::EventCategory(elem).get(), elem);
    }
}

TEST(EventInfoTest, EventCategory_from_json)
{
    for (const std::string& elem : event_info::EventCategory::valuesAllowed)
    {
        EXPECT_NO_THROW(nlohmann::json(elem).get<event_info::EventCategory>());
        EXPECT_EQ(nlohmann::json(elem).get<event_info::EventCategory>().get(),
                  elem);
    }
    EXPECT_ANY_THROW(nlohmann::json("junk").get<event_info::EventCategory>());
    EXPECT_ANY_THROW(nlohmann::json("").get<event_info::EventCategory>());
}

TEST(EventInfoTest, EventInfo_loadFrom)
{
    nlohmann::json js = event_GPU_VRFailure();
    event_info::EventNode event;
    EXPECT_NO_THROW(event.loadFrom(js));
    EXPECT_NO_THROW(event.getCategories());
    EXPECT_EQ(event.getCategories(),
              std::vector<event_info::EventCategory>{
                  event_info::EventCategory("power_rail")});
}

TEST(EventInfoTest, EventInfo_loadFrom1)
{
    nlohmann::json js = event_GPU_SpiFlashError();
    event_info::EventNode event;
    EXPECT_NO_THROW(event.loadFrom(js));
    EXPECT_NO_THROW(event.getCategories());
    EXPECT_EQ(event.getCategories(), std::vector<event_info::EventCategory>{});
}
