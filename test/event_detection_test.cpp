#include "aml.hpp"
#include "event_detection.hpp"
#include "message_composer.hpp"
#include "nlohmann/json.hpp"

#include "gmock/gmock.h"

event_info::EventMap *eventMap = nullptr;
event_handler::EventHandlerManager *eventHdlrMgr = nullptr;
/* Empty object needed just for calling isEvent method */
event_detection::EventDetection eventDetection(
    "EventDetection1", eventMap, eventHdlrMgr);

static bool setup_event_cnt_test(event_info::EventNode &ev, int tested_trigger_count, std::string device_name)
{
    nlohmann::json j;
    j["event"] = "Event0";
    j["device_type"] = "GPU";
    j["severity"] = "Critical";
    j["resolution"] = "Contact NVIDIA Support";
    j["redfish"]["message_id"] = "ResourceEvent.1.0.ResourceErrorsDetected";
    j["telemetries"] = {"t0", "t1"};
    j["trigger_count"] = tested_trigger_count;
    j["event_trigger"] = "trigger";
    j["action"] = "do something";
    j["event_counter_reset"]["type"] = "type";
    j["event_counter_reset"]["metadata"] = "metadata";
    j["accessor"]["metadata"] = "metadata";
    j["accessor"]["type"] = "DBUS";
    ev.loadFrom(j);
    ev.device = device_name;
    // ev.print();

    return true;
}

TEST(EventDetectionTest, TID_1_triggers_instantly)
{
    event_info::EventNode event("TID_1_triggers_instantly");
    int trigger_count = 0;
    EXPECT_EQ(setup_event_cnt_test(event, trigger_count, "GPU5"), true);

    EXPECT_EQ(eventDetection.IsEvent(event), true);
}

TEST(EventDetectionTest, TID_2_triggers_instantly)
{
    event_info::EventNode event("TID_2_triggers_instantly");
    int trigger_count = 1;
    EXPECT_EQ(setup_event_cnt_test(event, trigger_count, "GPU5"), true);

    EXPECT_EQ(eventDetection.IsEvent(event), true);
}

TEST(EventDetectionTest, TID_3_triggers_on_trigger_count)
{
    event_info::EventNode event("TID_3_triggers_on_trigger_count");
    int trigger_count = 5;
    EXPECT_EQ(setup_event_cnt_test(event, trigger_count, "GPU5"), true);

    for(auto i = 1; i < trigger_count; i++){
        EXPECT_EQ(eventDetection.IsEvent(event), false);
    }

    EXPECT_EQ(eventDetection.IsEvent(event), true);
    EXPECT_EQ(eventDetection.IsEvent(event), true);
    EXPECT_EQ(eventDetection.IsEvent(event), true);
}

TEST(EventDetectionTest, TID_4_counter_separation)
{
    event_info::EventNode ev1("TID_4_counter_separation");
    int ev1_trig_cnt = 1;
    EXPECT_EQ(setup_event_cnt_test(ev1, ev1_trig_cnt, "GPU1"), true);
    
    event_info::EventNode ev2("test event2");
    int ev2_trig_cnt = 3;
    EXPECT_EQ(setup_event_cnt_test(ev2, ev2_trig_cnt, "GPU2"), true);

    event_info::EventNode ev3("test event3");
    int ev3_trig_cnt = 9;
    EXPECT_EQ(setup_event_cnt_test(ev3, ev3_trig_cnt, "GPU3"), true);

    for(auto i = 1; i < 10; i++){
        EXPECT_EQ(eventDetection.IsEvent(ev1), (ev1_trig_cnt <= 1) ? (true) : (ev1_trig_cnt--, false));/* decrement counter and return false to ternary operator */
        EXPECT_EQ(eventDetection.IsEvent(ev2), (ev2_trig_cnt <= 1) ? (true) : (ev2_trig_cnt--, false));
        EXPECT_EQ(eventDetection.IsEvent(ev3), (ev3_trig_cnt <= 1) ? (true) : (ev3_trig_cnt--, false));
    }
}
