#include "gmock/gmock.h"
#include "event_info.hpp"
#include "nlohmann/json.hpp"
#include "message_composer.hpp"
#include "aml.hpp"

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
    EXPECT_EQ(event.redfishStruct.messageId, "ResourceEvent.1.0.ResourceErrorsDetected");
    EXPECT_EQ(event.eventCounterResetStruct.metadata, "metadata");
    EXPECT_EQ(event.redfishStruct.messageStruct.severity, "Critical");
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