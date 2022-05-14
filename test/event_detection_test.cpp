#include "aml.hpp"
#include "event_detection.hpp"
#include "message_composer.hpp"
#include "nlohmann/json.hpp"

#include "gmock/gmock.h"

static bool setup_event_cnt_test(event_info::EventNode& ev,
                                 int tested_trigger_count,
                                 const std::string& device_name)
{
    nlohmann::json j;
    j["event"] = "Event0";
    j["device_type"] = "GPU";
    j["severity"] = "Critical";
    j["resolution"] = "Contact NVIDIA Support";
    j["redfish"]["message_id"] = "ResourceEvent.1.0.ResourceErrorsDetected";
    j["telemetries"] = {"t0", "t1"};
    j["trigger_count"] = tested_trigger_count;
    j["event_trigger"]["metadata"] = "metadata";
    j["event_trigger"]["type"] = "DBUS";
    j["action"] = "do something";
    j["event_counter_reset"]["type"] = "type";
    j["event_counter_reset"]["metadata"] = "metadata";
    j["accessor"]["metadata"] = "metadata";
    j["accessor"]["type"] = "DBUS";
    j["value_as_count"] = false;
    ev.loadFrom(j);
    ev.device = device_name;
    // ev.print();

    return true;
}

TEST(EventLookupTest, TriggerAccessor)
{
    event_info::EventNode ev("Sample Event");
    nlohmann::json j;
    j["event"] = "Event0";
    j["device_type"] = "GPU";
    j["severity"] = "Critical";
    j["resolution"] = "Contact NVIDIA Support";
    j["redfish"]["message_id"] = "ResourceEvent.1.0.ResourceErrorsDetected";
    j["telemetries"] = {"t0", "t1"};
    j["trigger_count"] = 0;
    j["event_trigger"]["property"] = "property";
    j["event_trigger"]["type"] = "DBUS";
    j["action"] = "do something";
    j["event_counter_reset"]["type"] = "type";
    j["event_counter_reset"]["metadata"] = "metadata";
    j["accessor"]["property"] = "property";
    j["accessor"]["type"] = "DBUS";
    j["value_as_count"] = false;
    ev.loadFrom(j);

    event_info::EventNode ev2("Sample Event2");
    nlohmann::json j2;
    j2["event"] = "Event0";
    j2["device_type"] = "GPU";
    j2["severity"] = "Critical";
    j2["resolution"] = "Contact NVIDIA Support";
    j2["redfish"]["message_id"] = "ResourceEvent.1.0.ResourceErrorsDetected";
    j2["telemetries"] = {"t0", "t1"};
    j2["trigger_count"] = 0;
    j2["event_trigger"] = nlohmann::json::object();
    j2["action"] = "do something";
    j2["event_counter_reset"]["type"] = "type";
    j2["event_counter_reset"]["metadata"] = "metadata";
    j2["accessor"]["property"] = "property2";
    j2["accessor"]["type"] = "DBUS";
    j2["value_as_count"] = false;
    ev2.loadFrom(j2);

    std::vector<event_info::EventNode> v;
    v.push_back(ev);
    v.push_back(ev2);

    event_info::EventMap eventMap;

    eventMap.insert(
        std::pair<std::string, std::vector<event_info::EventNode>>("GPU", v));

    event_handler::EventHandlerManager* eventHdlrMgr = nullptr;

    event_detection::EventDetection eventDetection("EventDetection2", &eventMap,
                                                   eventHdlrMgr);

    ev.device = "GPU";
    EXPECT_EQ(eventDetection.IsEvent(ev), true);
    EXPECT_EQ(ev.count[ev.device], 0);

    ev.valueAsCount = true;
    EXPECT_EQ(eventDetection.IsEvent(ev, 10), true);

    ev.triggerCount = 20;
    EXPECT_EQ(eventDetection.IsEvent(ev, 10), false);

    ev.count[ev.device] = 10;
    ev.valueAsCount = false;
    EXPECT_EQ(eventDetection.IsEvent(ev), false);
    EXPECT_EQ(ev.count[ev.device], 11);
    ev.triggerCount = 0;

    nlohmann::json j3;
    j3["property"] = "property2";
    j3["type"] = "DBUS";
    data_accessor::DataAccessor accessor(j3);

    nlohmann::json j4;
    j4["property"] = "property";
    j4["type"] = "DBUS";
    data_accessor::DataAccessor accessorTrigger(j4);

    nlohmann::json j5;
    j5["property"] = "prop";
    j5["type"] = "DBUS";
    data_accessor::DataAccessor accessorNil(j5);

    EXPECT_NE(eventDetection.LookupEventFrom(accessor).empty(), true);
    EXPECT_NE(eventDetection.LookupEventFrom(accessorTrigger).empty(), true);
    EXPECT_EQ(eventDetection.LookupEventFrom(accessorNil).empty(), true);
}

TEST(EventDetectionTest, TID_1_triggers_instantly)
{
    event_info::EventMap* eventMap = nullptr;
    event_handler::EventHandlerManager* eventHdlrMgr = nullptr;
    event_detection::EventDetection eventDetection("EventDetection1", eventMap,
                                                   eventHdlrMgr);
    event_info::EventNode event("TID_1_triggers_instantly");
    int trigger_count = 0;
    EXPECT_EQ(setup_event_cnt_test(event, trigger_count, "GPU5"), true);

    EXPECT_EQ(eventDetection.IsEvent(event), true);
}

TEST(EventDetectionTest, TID_2_triggers_instantly)
{
    event_info::EventMap* eventMap = nullptr;
    event_handler::EventHandlerManager* eventHdlrMgr = nullptr;
    event_detection::EventDetection eventDetection("EventDetection1", eventMap,
                                                   eventHdlrMgr);
    event_info::EventNode event("TID_2_triggers_instantly");
    int trigger_count = 1;
    EXPECT_EQ(setup_event_cnt_test(event, trigger_count, "GPU5"), true);

    EXPECT_EQ(eventDetection.IsEvent(event), true);
}

TEST(EventDetectionTest, TID_3_triggers_on_trigger_count)
{
    event_info::EventMap* eventMap = nullptr;
    event_handler::EventHandlerManager* eventHdlrMgr = nullptr;
    event_detection::EventDetection eventDetection("EventDetection1", eventMap,
                                                   eventHdlrMgr);
    event_info::EventNode event("TID_3_triggers_on_trigger_count");
    int trigger_count = 5;
    EXPECT_EQ(setup_event_cnt_test(event, trigger_count, "GPU5"), true);

    for (auto i = 1; i < trigger_count; i++)
    {
        EXPECT_EQ(eventDetection.IsEvent(event), false);
    }

    EXPECT_EQ(eventDetection.IsEvent(event), true);
    EXPECT_EQ(eventDetection.IsEvent(event), true);
    EXPECT_EQ(eventDetection.IsEvent(event), true);
}

TEST(EventDetectionTest, TID_4_counter_separation)
{
    event_info::EventMap* eventMap = nullptr;
    event_handler::EventHandlerManager* eventHdlrMgr = nullptr;
    event_detection::EventDetection eventDetection("EventDetection1", eventMap,
                                                   eventHdlrMgr);
    event_info::EventNode ev1("TID_4_counter_separation");
    int ev1_trig_cnt = 1;
    EXPECT_EQ(setup_event_cnt_test(ev1, ev1_trig_cnt, "GPU1"), true);

    event_info::EventNode ev2("test event2");
    int ev2_trig_cnt = 3;
    EXPECT_EQ(setup_event_cnt_test(ev2, ev2_trig_cnt, "GPU2"), true);

    event_info::EventNode ev3("test event3");
    int ev3_trig_cnt = 9;
    EXPECT_EQ(setup_event_cnt_test(ev3, ev3_trig_cnt, "GPU3"), true);

    for (auto i = 1; i < 10; i++)
    {
        /* decrement counter and return false to ternary operator */
        EXPECT_EQ(eventDetection.IsEvent(ev1),
                  (ev1_trig_cnt <= 1) ? (true) : (ev1_trig_cnt--, false));
        EXPECT_EQ(eventDetection.IsEvent(ev2),
                  (ev2_trig_cnt <= 1) ? (true) : (ev2_trig_cnt--, false));
        EXPECT_EQ(eventDetection.IsEvent(ev3),
                  (ev3_trig_cnt <= 1) ? (true) : (ev3_trig_cnt--, false));
    }
}
