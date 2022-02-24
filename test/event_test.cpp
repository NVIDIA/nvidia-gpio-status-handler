#include "aml.hpp"
#include "event_detection.hpp"
#include "event_info.hpp"
#include "message_composer.hpp"
#include "nlohmann/json.hpp"

#include "gmock/gmock.h"

TEST(EventTest, LoadJson)
{
    nlohmann::json j;
    j["event"] = "Event0";
    j["device_type"] = "GPU";
    j["severity"] = "Critical";
    j["resolution"] = "Contact NVIDIA Support";
    j["redfish"]["message_id"] = "ResourceEvent.1.0.ResourceErrorsDetected";
    j["telemetries"] = {"t0", "t1"};
    j["trigger_count"] = 0;
    j["event_trigger"] = "trigger";
    j["action"] = "do something";
    j["event_counter_reset"]["type"] = "type";
    j["event_counter_reset"]["metadata"] = "metadata";
    j["accessor"]["metadata"] = "metadata";
    j["accessor"]["type"] = "DBUS";
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
    j["telemetries"] = {"t0", "t1"};
    j["trigger_count"] = 0;
    j["event_trigger"] = "trigger";
    j["action"] = "do something";
    j["event_counter_reset"]["type"] = "type";
    j["event_counter_reset"]["metadata"] = "metadata";
    j["accessor"]["metadata"] = "metadata";
    j["accessor"]["type"] = "DBUS";
    event_info::EventNode event("test event");
    event.loadFrom(j);
    message_composer::MessageComposer mc("Test Msg Composer");
    EXPECT_EQ(mc.getName(), "Test Msg Composer");
}

TEST(DevNameTest, MakeCall)
{
    auto objPath = "/xyz/openbmc_project/inventory/system/chassis/GPU12";
    auto devType = "GPU";
    auto name =
        event_detection::EventDetection::DetermineDeviceName(objPath, devType);
    EXPECT_EQ(name, "GPU12");
}