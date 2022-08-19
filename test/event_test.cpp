#include "aml.hpp"
#include "dat_traverse.hpp"
#include "event_detection.hpp"
#include "event_info.hpp"
#include "message_composer.hpp"
#include "nlohmann/json.hpp"
#include "util.hpp"

#include "gmock/gmock.h"

TEST(EventTest, LoadJson)
{
    nlohmann::json j;
    j["event"] = "Event0";
    j["device_type"] = "GPU";
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
    EXPECT_EQ(event.messageRegistry.messageId,
              "ResourceEvent.1.0.ResourceErrorsDetected");
    EXPECT_EQ(event.counterReset["metadata"], "metadata");
    EXPECT_EQ(event.messageRegistry.message.severity, "Critical");
}

TEST(MsgCompTest, MakeCall)
{
    nlohmann::json j;
    j["event"] = "Event0";
    j["device_type"] = "GPU";
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
    auto objPath = "/xyz/openbmc_project/inventory/system/chassis/GPU12";
    auto devType = "GPU";
    auto name = util::determineDeviceName(objPath, devType);
    EXPECT_EQ(name, "GPU12");
}

TEST(EventTelemtries, MakeCall)
{
    nlohmann::json j;
    j["event"] = "Event0";
    j["device_type"] = "GPU";
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
    EXPECT_EQ(jCollected.size(), 3);
    EXPECT_EQ(jCollected["power"], "123");
    EXPECT_EQ(jCollected["temperature"], "123");
    EXPECT_EQ(jCollected["selftest"].size(), 0);
}
