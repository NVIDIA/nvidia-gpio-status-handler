#include "check_accessor.hpp"
#include "data_accessor.hpp"
#include "check_accessor.hpp"
#include "data_accessor.hpp"
#include "nlohmann/json.hpp"
#include "property_accessor.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <tuple>

#include <gtest/gtest.h>

using namespace data_accessor;

// it contains the 'check' field
const nlohmann::json jsonBITMASK = {
    {"type", "DBUS"},
    {"object", "/xyz/openbmc_project/inventory/system/chassis/GPU[0-7]"},
    {"interface", "xyz.openbmc_project.Inventory.Decorator.Dimension"},
    {"property", "Depth"},
    {"check", {{"bitmask", "0x01"}}}};

// it DOES NOT contain the 'check' field
const nlohmann::json jsonDEVICE = {
    {"type", "DBUS"},
    {"object", "/xyz/openbmc_project/inventory/system/chassis/GPU0"},
    {"interface", "xyz.openbmc_project.Inventory.Decorator.Dimension"},
    {"property", "Depth"}};

TEST(DataAccessor, ContainsPositive)
{
    DataAccessor accessorEVENT{jsonBITMASK};
    DataAccessor accessorPROPERTY{jsonDEVICE};
    EXPECT_EQ(accessorEVENT.contains(accessorPROPERTY), true);
}

TEST(DataAccessor, ContainsNegative)
{
    DataAccessor accessorEVENT{jsonBITMASK};
    DataAccessor accessorPROPERTY{jsonDEVICE};
    EXPECT_NE(accessorPROPERTY.contains(accessorEVENT), true);
}

TEST(DataAccessor, CheckPositiveNoCheckField)
{
    DataAccessor accessorWITHOUTCheck{jsonDEVICE};
    DataAccessor accessorPROPERTY{jsonDEVICE};
    CheckAccessor accCheck{""};
    EXPECT_EQ(accCheck.check(accessorWITHOUTCheck, accessorPROPERTY), true);
}

TEST(DataAccessor, CheckPositiveCMDLINE)
{
    // command 'mctp-vdm-util'  output: ff 00 00 00 00 00 02 40 66 28
    const nlohmann::json jsonCMDLINE = {
        {"type", "CMDLINE"},
        {"executable", "/bin/echo"},
        {"arguments", "ff 00 00 00 00 00 02 40 66 28"},
        {"check", {{"lookup", "00 02 40"}}}};
    DataAccessor cmdAccessor{jsonCMDLINE};
    CheckAccessor accCheck{""};
    EXPECT_EQ(accCheck.check(cmdAccessor, cmdAccessor), true);
}

TEST(DataAccessor, CheckNegativeCMDLINE)
{
    // command 'mctp-vdm-util'  output: ff 00 00 00 00 00 02 40 66 28
    const nlohmann::json jsonCMDLINE = {
        {"type", "CMDLINE"},
        {"executable", "/bin/echo"},
        {"arguments", "ff 00 00 00 00 00 02 40 66 28"},
        {"check", {{"lookup", "_doesNotExist_"}}}};
    DataAccessor cmdAccessor{jsonCMDLINE};
    CheckAccessor accCheck{""};
    EXPECT_NE(accCheck.check(cmdAccessor, cmdAccessor), true);
}

TEST(DataAccessor, CheckNegativeExecutableDoesNotExist)
{
    const nlohmann::json jsonCMDLINE = {
        {"type", "CMDLINE"},
        {"executable", "/bin/_binary_does_not_exist"},
        {"check", {{"lookup", "_doesNotExist_"}}}};
    DataAccessor cmdAccessor{jsonCMDLINE};
    CheckAccessor accCheck{""};
    EXPECT_NE(accCheck.check(cmdAccessor, cmdAccessor), true);
}

TEST(DataAccessor, CheckPositiveScriptCMDLINE)
{
    // command 'mctp-vdm-util'  output: ff 00 00 00 00 00 02 40 66 28
    constexpr auto filename = "./mctp-vdm-util";

    const nlohmann::json jsonCMDLINE = {
        {"type", "CMDLINE"},
        {"executable", filename},
        {"arguments", "-c query_boot_status -t 32"},
        {"check", {{"lookup", "00 02 40"}}}};

    // create file mctp-vdm-util
    std::ofstream mctp(filename);
    mctp << "#!/bin/sh" << std::endl
         << "echo ff 00 00 00 00 00 02 40 66 28" << std::endl;
    mctp.close();

    // make filename executable
    std::filesystem::permissions(filename,
                                 std::filesystem::perms::owner_all |
                                     std::filesystem::perms::group_all,
                                 std::filesystem::perm_options::add);

    DataAccessor cmdAccessor{jsonCMDLINE};
    CheckAccessor accCheck{""};
    bool result = accCheck.check(cmdAccessor, cmdAccessor);
    const std::filesystem::path filenamepath = filename;
    std::filesystem::remove(filenamepath);

    EXPECT_EQ(result, true);
}

TEST(DataAccessor, CheckPositiveBitmaskRedefinition)
{
    DataAccessor jAccessor(jsonBITMASK); // {"bitmask", "0x01"}
    DataAccessor dAccessor(PropertyVariant(0x10f)); // bits 0,1,2,3 and 8

    // without redefinition
    CheckAccessor accCheck{""};
    EXPECT_EQ(accCheck.check(jAccessor, dAccessor), true); // bit 0

    // with redefinition, bits 1-3 pass
    const nlohmann::json jsonBit1 = {{"check", {{"bitmask", "0x02"}}}};
    const nlohmann::json jsonBit2 = {{"check", {{"bitmask", "0x04"}}}};
    const nlohmann::json jsonBit3 = {{"check", {{"bitmask", "0x08"}}}};
    EXPECT_EQ(accCheck.check(DataAccessor(jsonBit1), dAccessor), true);
    EXPECT_EQ(accCheck.check(DataAccessor(jsonBit2), dAccessor), true);
    EXPECT_EQ(accCheck.check(DataAccessor(jsonBit3), dAccessor), true);

    // bits 4-7 fail
    const nlohmann::json jsonBit4 = {{"check", {{"bitmask", "0x10"}}}};
    const nlohmann::json jsonBit5 = {{"check", {{"bitmask", "0x20"}}}};
    const nlohmann::json jsonBit6 = {{"check", {{"bitmask", "0x40"}}}};
    const nlohmann::json jsonBit7 = {{"check", {{"bitmask", "0x80"}}}};
    EXPECT_NE(accCheck.check(DataAccessor(jsonBit4), dAccessor), true);
    EXPECT_NE(accCheck.check(DataAccessor(jsonBit5), dAccessor), true);
    EXPECT_NE(accCheck.check(DataAccessor(jsonBit6), dAccessor), true);
    EXPECT_NE(accCheck.check(DataAccessor(jsonBit7), dAccessor), true);

    // finally bit 8 passes
    const nlohmann::json jsonBit8 = {{"check", {{"bitmask", "0x100"}}}};
    EXPECT_EQ(accCheck.check(DataAccessor(jsonBit8), dAccessor), true);
}

//TEST(DataAccessor, CheckPositiveLookupRedefinition)
//{
//    const nlohmann::json json = {{"type", "CMDLINE"},
//                                 {"executable", "/bin/echo"},
//                                 {"arguments", "ff 00 00 00 00 00 02 40 66 28"},
//                                 {"check", {{"lookup", "_doesNotExist_"}}}};
//    DataAccessor jAccessor(json);

//    // not found without redefenition
//    EXPECT_NE(jAccessor.check(), true);
//    // found with redefinition
//    EXPECT_EQ(jAccessor.check(PropertyVariant(std::string{"40 6"})), true);
//    EXPECT_EQ(jAccessor.check(PropertyVariant(std::string{"ff 0"})), true);
//    EXPECT_EQ(jAccessor.check(PropertyVariant(std::string{"6 28"})), true);
//    // redefinition whennot found
//    EXPECT_NE(jAccessor.check(PropertyVariant(std::string{"zz"})), true);
//}

TEST(DataAccessor, EqualPositiveRegexAgainstNoRegex)
{
    const nlohmann::json jsonFile = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/inventory/system/memory/GPU[0-7]"}};

    const nlohmann::json dbusProperty = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/inventory/system/memory/GPU0"}};

    DataAccessor jsonAccessor(jsonFile);
    DataAccessor objectAccessor(dbusProperty);
    EXPECT_EQ(jsonAccessor == objectAccessor, true);
}

TEST(DataAccessor, EqualNegativeNoRegxAgainstRegex)
{
    const nlohmann::json jsonFile = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/inventory/system/memory/GPU0"}};

    const nlohmann::json dbusProperty = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/inventory/system/memory/GPU[0-7]"}};

    DataAccessor jsonAccessor(jsonFile);
    DataAccessor objectAccessor(dbusProperty);
    EXPECT_NE(jsonAccessor == objectAccessor, true);
}

TEST(DataAccessor, EqualPositiveTypeOnly)
{
    const nlohmann::json jsonFile = {{"type", "DBUS"}};

    const nlohmann::json dbusProperty = {{"type", "DBUS"}};

    DataAccessor jsonAccessor(jsonFile);
    DataAccessor objectAccessor(dbusProperty);
    EXPECT_EQ(jsonAccessor == objectAccessor, true);
}

TEST(DataAccessor, EqualPositiveNoRegxAgainstNoRegex)
{
    const nlohmann::json jsonFile = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/inventory/system/memory/GPU0"}};

    const nlohmann::json dbusProperty = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/inventory/system/memory/GPU0"}};

    DataAccessor jsonAccessor(jsonFile);
    DataAccessor objectAccessor(dbusProperty);
    EXPECT_EQ(jsonAccessor == objectAccessor, true);
}

TEST(DataAccessor, EqualPositiveWithSkippedCheckField)
{
    const nlohmann::json jsonFile = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/inventory/system/memory/GPU0"}};

    const nlohmann::json dbusProperty = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/inventory/system/memory/GPU0"},
        {"check", {{"bitmask", "0x01"}}}};

    DataAccessor jsonAccessor(jsonFile);
    DataAccessor objectAccessor(dbusProperty);
    EXPECT_EQ(jsonAccessor == objectAccessor, true);
}

TEST(DataAccessor, EqualNegativeRegexAgainstNoRegex)
{
    const nlohmann::json jsonFile = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/inventory/system/memory/GPU[0-7]"}};

    const nlohmann::json dbusProperty = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/inventory/system/memory/GPU"}};

    DataAccessor jsonAccessor(jsonFile);
    DataAccessor objectAccessor(dbusProperty);
    EXPECT_NE(jsonAccessor == objectAccessor, true);
}

TEST(DataAccessor, EqualNegativeDifferentTypes)
{
    const nlohmann::json jsonFile = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/inventory/system/memory/GPU[0-7]"}};

    const nlohmann::json dbusProperty = {
        {"type", "DBUS call"},
        {"object", "/xyz/openbmc_project/inventory/system/memory/GPU0"}};

    DataAccessor jsonAccessor(jsonFile);
    DataAccessor objectAccessor(dbusProperty);
    EXPECT_NE(jsonAccessor == objectAccessor, true);
}

TEST(DataAccessor, EqualNegativeNoTypeInThis)
{
    const nlohmann::json jsonFile = {
        {"object", "/xyz/openbmc_project/inventory/system/memory/GPU[0-7]"}};

    const nlohmann::json dbusProperty = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/inventory/system/memory/GPU0"}};

    DataAccessor jsonAccessor(jsonFile);
    DataAccessor objectAccessor(dbusProperty);
    EXPECT_NE(jsonAccessor == objectAccessor, true);
}

TEST(DataAccessor, EqualNegativeNoTypeInOther)
{
    const nlohmann::json jsonFile = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/inventory/system/memory/GPU[0-7]"}};

    const nlohmann::json dbusProperty = {
        {"object", "/xyz/openbmc_project/inventory/system/memory/GPU0"}};

    DataAccessor jsonAccessor(jsonFile);
    DataAccessor objectAccessor(dbusProperty);
    EXPECT_NE(jsonAccessor == objectAccessor, true);
}

TEST(DataAccessor, EqualOperatorNegativeCompleteAccessorDiffObject)
{
    const nlohmann::json jsonFile = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/inventory/system/memory/GPU[0-7]"},
        {"interface", "xyz.openbmc_project.Memory.MemoryECC"},
        {"property", "ceCount"}};

    const nlohmann::json dbusProperty = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/inventory/system/memory/GPUDRAM0"},
        {"interface", "xyz.openbmc_project.Memory.MemoryECC"},
        {"property", "ceCount"}};

    DataAccessor jsonAccessor(jsonFile);
    DataAccessor objectAccessor(dbusProperty);

    EXPECT_NE(jsonAccessor == objectAccessor, true);
}

TEST(DataAccessor, EqualOperatorNegativeCompleteAccessorDiffInterface)
{
    const nlohmann::json jsonFile = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/inventory/system/memory/GPUDRAM[0-7]"},
        {"interface", "xyz.openbmc_project.Memory.MemoryECC.Different"},
        {"property", "ceCount"}};

    const nlohmann::json dbusProperty = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/inventory/system/memory/GPUDRAM0"},
        {"interface", "xyz.openbmc_project.Memory.MemoryECC"},
        {"property", "ceCount"}};

    DataAccessor jsonAccessor(jsonFile);
    DataAccessor objectAccessor(dbusProperty);

    EXPECT_NE(jsonAccessor == objectAccessor, true);
}

TEST(DataAccessor, EqualOperatorNegativeCompleteAccessorDiffProperty)
{
    const nlohmann::json jsonFile = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/inventory/system/memory/GPUDRAM[0-7]"},
        {"interface", "xyz.openbmc_project.Memory.MemoryECC"},
        {"property", "ceCount"}};

    const nlohmann::json dbusProperty = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/inventory/system/memory/GPUDRAM0"},
        {"interface", "xyz.openbmc_project.Memory.MemoryECC"},
        {"property", "ceCount_changed"}};

    DataAccessor jsonAccessor(jsonFile);
    DataAccessor objectAccessor(dbusProperty);

    EXPECT_NE(jsonAccessor == objectAccessor, true);
}

#if 0 // these tests depend on DBUS, TODO: use Goggle Mock
TEST(DataAccessor, EventTriggerForOverTemperature)
{
    const nlohmann::json eventTrigger = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/GpioStatusHandler"},
        {"interface", "xyz.openbmc_project.GpioStatus"},
        {"property", "THERM_OVERT"},
        {"check", {{"equal", "true"}}}};

    const nlohmann::json accessor = {
        {"type", "DeviceCoreAPI"},
        {"property", "gpu.thermal.temperature.overTemperatureInfo"},
        {"check", {{"bitmap", "1"}}}};

    DataAccessor triggerAccessor{eventTrigger};
    DataAccessor accessorAccessor{accessor};

    DataAccessor dataForEventTrigger(PropertyVariant(bool(true)));
    auto ok = triggerAccessor.check(dataForEventTrigger, accessorAccessor,
                                    "GPU[0-7]");
    EXPECT_EQ(ok, true);

    auto devices = dataForEventTrigger.getAssertedDevices();
    EXPECT_EQ(devices.empty(), false);
}

TEST(DataAccessor, EventTriggerForDestinationFlaTranslationError)
{
    const nlohmann::json eventTrigger = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/inventory/system/chassis/GPU0"},
        {"interface", "xyz.openbmc_project.com.nvidia.Events.PendingRegister"},
        {"property", "EventsPendingRegister"},
        {"check", {{"bitmask", "4"}}}};

    const nlohmann::json accessor = {{"type", "DeviceCoreAPI"},
                                     {"property", "gpu.xid.event"},
                                     {"check", {{"lookup", "30"}}}};

    DataAccessor triggerAccessor{eventTrigger};
    DataAccessor accessorAccessor{accessor};

    const std::string deviceType{"GPUDRAM[0-7]"};
    DataAccessor dataForEventTrigger(PropertyVariant(uint64_t(0x04)));

    auto ok = triggerAccessor.check(dataForEventTrigger, deviceType);
    EXPECT_EQ(ok, true);

    ok = accessorAccessor.check(deviceType);
    EXPECT_EQ(ok, true);

    auto devices = dataForEventTrigger.getAssertedDevices();
    if (devices.empty() == true)
    {
        devices = dataForEventTrigger.getAssertedDevices();
    }
    EXPECT_EQ(devices.size(), 1);
    EXPECT_EQ(devices.count(0), 1);
    EXPECT_EQ(devices[0], "GPU0");
}
#endif

TEST(DataAccessor, BitmaskWithValueTwo)
{
    const nlohmann::json templateAccessor = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/inventory/system/chassis/GPU[0-7]"},
        {"interface", "xyz.openbmc_project.com.nvidia.Events.PendingRegister"},
        {"property", "EventsPendingRegister"},
        {"check", {{"bitmask", "2"}}}};

    const nlohmann::json eventTrigger = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/inventory/system/chassis/GPU0"},
        {"interface", "xyz.openbmc_project.com.nvidia.Events.PendingRegister"},
        {"property", "EventsPendingRegister"}};


    DataAccessor accessor(templateAccessor);
    DataAccessor accessorData(eventTrigger, PropertyValue(uint64_t(0x02)));

    const std::string deviceType{"GPU[0-7]"};
    CheckAccessor accCheck(deviceType);
    bool ok = accCheck.check(accessor, accessorData);
    EXPECT_EQ(ok, true);
    auto devices = accCheck.getAssertedDevices();
    if (devices.empty() == false)
    {
        EXPECT_EQ(devices.size(), 1);
        device_id::PatternIndex gpuId(0);
        EXPECT_EQ(devices[0].deviceIndexTuple, gpuId);
        EXPECT_EQ(devices[0].device, "GPU0");
    }
}

TEST(DataAccessor, BitmapWithoutRangeInDeviceType)
{
    const nlohmann::json triggerJson = {
        {"type", "DBUS"},
        {"object", "/xyz/openbmc_project/GpioStatusHandler"},
        {"interface", "xyz.openbmc_project.GpioStatus"},
        {"property", "I2C4_ALERT"},
        {"check", {{"equal", "true"}}}};

    const std::string deviceType{"PCIeSwitch"};

    DataAccessor triggerAccessor(triggerJson);
    DataAccessor dataTriggerAccessor(PropertyVariant(std::string{"true"}));

    const nlohmann::json jsonAccessor = {
        {"type", "DeviceCoreAPI"},
        {"property", "pcieswitch.0V8.abnormalPowerChange"},
        {"check", {{"bitmap", "1"}}}};

    DataAccessor accessorFromJson(jsonAccessor);
    DataAccessor dataAccessorAccessor(PropertyVariant(uint64_t(0x01)));
    CheckAccessor triggerCheck(deviceType), accCheck(deviceType);

    auto ok =
        triggerCheck.subCheck(triggerAccessor, dataTriggerAccessor, deviceType);
    EXPECT_EQ(ok, true);
    auto devicesEvtTrigg = triggerCheck.getAssertedDevices();

    ok = accCheck.subCheck(accessorFromJson, dataAccessorAccessor, deviceType);
    EXPECT_EQ(ok, true);
    auto devices = accCheck.getAssertedDevices();
    if (devices.empty() == true)
    {
        devices = devicesEvtTrigg;
    }
    EXPECT_EQ(devices.size(), 1);
    EXPECT_EQ(devices[0].device, "PCIeSwitch");
}

TEST(DataAccessor, CopyOperator)
{
    DataAccessor source(PropertyVariant(int(1)));
    DataAccessor destination;
    EXPECT_EQ(destination.hasData(), false);
    EXPECT_EQ(destination.getDataValue().empty(), true);
    destination = source;
    EXPECT_NE(destination.hasData(), false);
    EXPECT_NE(destination.getDataValue().empty(), true);
    EXPECT_EQ(destination.getDataValue().isValidInteger(), true);
    EXPECT_EQ(destination.getDataValue().getInteger(), 1);
}
