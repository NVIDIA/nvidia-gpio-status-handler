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
    j["sub_type"] = "";
    j["severity"] = "Critical";
    j["resolution"] = "Contact NVIDIA Support";
    j["redfish"]["message_id"] = "ResourceEvent.1.0.ResourceErrorsDetected";
    j["redfish"]["message_args"]["patterns"] = {"p1", "p2"};
    j["redfish"]["message_args"]["parameters"] = nlohmann::json::array();
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
    j["sub_type"] = "";
    j["severity"] = "Critical";
    j["resolution"] = "Contact NVIDIA Support";
    j["redfish"]["message_id"] = "ResourceEvent.1.0.ResourceErrorsDetected";
    j["redfish"]["message_args"]["patterns"] = {"p1", "p2"};
    j["redfish"]["message_args"]["parameters"] = nlohmann::json::array();
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
    j2["sub_type"] = "";
    j2["severity"] = "Critical";
    j2["resolution"] = "Contact NVIDIA Support";
    j2["redfish"]["message_id"] = "ResourceEvent.1.0.ResourceErrorsDetected";
    j2["redfish"]["message_args"]["patterns"] = {"p1", "p2"};
    j2["redfish"]["message_args"]["parameters"] = nlohmann::json::array();
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

    event_info::PropertyFilterSet propertyFilterSet;

    event_handler::EventHandlerManager* eventHdlrMgr = nullptr;

    event_detection::EventDetection eventDetection("EventDetection2", &eventMap,
                                                   &propertyFilterSet,
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
    event_info::PropertyFilterSet* propertyFilterSet = nullptr;
    event_handler::EventHandlerManager* eventHdlrMgr = nullptr;
    event_detection::EventDetection eventDetection("EventDetection1", eventMap,
                                                   propertyFilterSet,
                                                   eventHdlrMgr);
    event_info::EventNode event("TID_1_triggers_instantly");
    int trigger_count = 0;
    EXPECT_EQ(setup_event_cnt_test(event, trigger_count, "GPU5"), true);

    EXPECT_EQ(eventDetection.IsEvent(event), true);
}

TEST(EventDetectionTest, TID_2_triggers_instantly)
{
    event_info::EventMap* eventMap = nullptr;
    event_info::PropertyFilterSet* propertyFilterSet = nullptr;
    event_handler::EventHandlerManager* eventHdlrMgr = nullptr;
    event_detection::EventDetection eventDetection("EventDetection1", eventMap,
                                                   propertyFilterSet,
                                                   eventHdlrMgr);
    event_info::EventNode event("TID_2_triggers_instantly");
    int trigger_count = 1;
    EXPECT_EQ(setup_event_cnt_test(event, trigger_count, "GPU5"), true);

    EXPECT_EQ(eventDetection.IsEvent(event), true);
}

TEST(EventDetectionTest, TID_3_triggers_on_trigger_count)
{
    event_info::EventMap* eventMap = nullptr;
    event_info::PropertyFilterSet* propertyFilterSet = nullptr;
    event_handler::EventHandlerManager* eventHdlrMgr = nullptr;
    event_detection::EventDetection eventDetection("EventDetection1", eventMap,
                                                   propertyFilterSet,
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
    event_info::PropertyFilterSet* propertyFilterSet = nullptr;
    event_handler::EventHandlerManager* eventHdlrMgr = nullptr;
    event_detection::EventDetection eventDetection("EventDetection1", eventMap,
                                                   propertyFilterSet,
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

TEST(EventDetectionTest, PropertyFilterMap)
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
    j["telemetries"] = nlohmann::json::array();
    j["trigger_count"] = 0;
    j["event_trigger"] = "trigger";
    j["action"] = "do something";
    j["event_counter_reset"]["type"] = "type";
    j["event_counter_reset"]["metadata"] = "metadata";
    j["accessor"]["type"] = "DBUS";
    j["accessor"]["object"] = "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_[1-8]/Ports/NVLink_[0-17]";
    j["accessor"]["interface"] = "xyz.openbmc_project.Inventory.Item.Port";
    j["accessor"]["property"] = "RecoveryCount";
    j["value_as_count"] = false;
    event_info::EventNode event("test event");
    event.loadFrom(j);
    event_info::PropertyFilterSet propertyFilterSet;
    event_info::addEventToPropertyFilterSet(event, propertyFilterSet);

    event_info::EventMap* eventMap = nullptr;
    event_handler::EventHandlerManager* eventHdlrMgr = nullptr;
    event_detection::EventDetection eventDetection("EventDetection1", eventMap,
                                                   &propertyFilterSet,
                                                   eventHdlrMgr);

    {
        nlohmann::json match_both;
        match_both["type"] = "DBUS";
        match_both["object"] = "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_1/Ports/NVLink_7";
        match_both["interface"] = "xyz.openbmc_project.Inventory.Item.Port";
        match_both["property"] = "RecoveryCount";
        data_accessor::DataAccessor acc_match_both(match_both);
        // EXPECT_TRUE(event_info::EventNode::getIsAccessorInteresting(event, acc_match_both));
        EXPECT_TRUE(eventDetection.getIsAccessorInteresting(acc_match_both));
    }

    {
        nlohmann::json match_both_2;
        match_both_2["type"] = "DBUS";
        match_both_2["object"] = "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_6/Ports/NVLink_2";
        match_both_2["interface"] = "xyz.openbmc_project.Inventory.Item.Port";
        match_both_2["property"] = "RecoveryCount";
        data_accessor::DataAccessor acc_match_both_2(match_both_2);
        // EXPECT_TRUE(event_info::EventNode::getIsAccessorInteresting(event, acc_match_both_2));
        EXPECT_TRUE(eventDetection.getIsAccessorInteresting(acc_match_both_2));
    }

    {
        nlohmann::json match_first;
        match_first["type"] = "DBUS";
        match_first["object"] = "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_1/Ports/SomethingElse";
        match_first["interface"] = "xyz.openbmc_project.Inventory.Item.Port";
        match_first["property"] = "RecoveryCount";
        data_accessor::DataAccessor acc_match_first(match_first);
        // EXPECT_FALSE(event_info::EventNode::getIsAccessorInteresting(event, acc_match_first));
        EXPECT_FALSE(eventDetection.getIsAccessorInteresting(acc_match_first));
    }

    {
        nlohmann::json match_second;
        match_second["type"] = "DBUS";
        match_second["object"] = "/xyz/openbmc_project/inventory/system/processors/NVSwitch_0/Ports/NVLink_7";
        match_second["interface"] = "xyz.openbmc_project.Inventory.Item.Port";
        match_second["property"] = "RecoveryCount";
        data_accessor::DataAccessor acc_match_second(match_second);
        // EXPECT_FALSE(event_info::EventNode::getIsAccessorInteresting(event, acc_match_second));
        EXPECT_FALSE(eventDetection.getIsAccessorInteresting(acc_match_second));
    }

    {
        nlohmann::json match_neither;
        match_neither["type"] = "DBUS";
        match_neither["object"] = "/xyz/openbmc_project/inventory/system/processors/NVSwitch_0/Ports/Something";
        match_neither["interface"] = "xyz.openbmc_project.Inventory.Item.Port";
        match_neither["property"] = "RecoveryCount";
        data_accessor::DataAccessor acc_match_neither(match_neither);
        // EXPECT_FALSE(event_info::EventNode::getIsAccessorInteresting(event, acc_match_neither));
        EXPECT_FALSE(eventDetection.getIsAccessorInteresting(acc_match_neither));
    }

    {
        nlohmann::json match_bad_range;
        match_bad_range["type"] = "DBUS";
        match_bad_range["object"] = "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_12/Ports/NVLink_80";
        match_bad_range["interface"] = "xyz.openbmc_project.Inventory.Item.Port";
        match_bad_range["property"] = "RecoveryCount";
        data_accessor::DataAccessor acc_match_bad_range(match_bad_range);
        // EXPECT_FALSE(event_info::EventNode::getIsAccessorInteresting(event, acc_match_bad_range));
        EXPECT_FALSE(eventDetection.getIsAccessorInteresting(acc_match_bad_range));
    }

    {
        nlohmann::json wrong_prefix;
        wrong_prefix["type"] = "DBUS";
        wrong_prefix["object"] = "/xyz/openbmc_project/inventory/SOMETHING/ELSE/GPU_SXM_1/Ports/NVLink_7";
        wrong_prefix["interface"] = "xyz.openbmc_project.Inventory.Item.Port";
        wrong_prefix["property"] = "RecoveryCount";
        data_accessor::DataAccessor acc_wrong_prefix(wrong_prefix);
        // EXPECT_FALSE(event_info::EventNode::getIsAccessorInteresting(event, acc_wrong_prefix));
        EXPECT_FALSE(eventDetection.getIsAccessorInteresting(acc_wrong_prefix));
    }

    {
        nlohmann::json wrong_iface;
        wrong_iface["type"] = "DBUS";
        wrong_iface["object"] = "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_1/Ports/NVLink_7";
        wrong_iface["interface"] = "xyz.openbmc_project.Inventory.Item.Port12345";
        wrong_iface["property"] = "RecoveryCount";
        data_accessor::DataAccessor acc_wrong_iface(wrong_iface);
        // EXPECT_FALSE(event_info::EventNode::getIsAccessorInteresting(event, acc_wrong_iface));
        EXPECT_FALSE(eventDetection.getIsAccessorInteresting(acc_wrong_iface));
    }

    {
        nlohmann::json wrong_prop;
        wrong_prop["type"] = "DBUS";
        wrong_prop["object"] = "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_1/Ports/NVLink_7";
        wrong_prop["interface"] = "xyz.openbmc_project.Inventory.Item.Port";
        wrong_prop["property"] = "SomethingElse";
        data_accessor::DataAccessor acc_wrong_prop(wrong_prop);
        // EXPECT_FALSE(event_info::EventNode::getIsAccessorInteresting(event, acc_wrong_prop));
        EXPECT_FALSE(eventDetection.getIsAccessorInteresting(acc_wrong_prop));
    }
}
