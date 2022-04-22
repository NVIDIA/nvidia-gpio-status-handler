#include "data_accessor.hpp"
#include "nlohmann/json.hpp"
#include "property_accessor.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

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
    EXPECT_EQ(accessorWITHOUTCheck.check(accessorPROPERTY), true);
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
    EXPECT_EQ(cmdAccessor.check(cmdAccessor), true);
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
    EXPECT_NE(cmdAccessor.check(cmdAccessor), true);
}

TEST(DataAccessor, CheckNegativeExecutableDoesNotExist)
{
    const nlohmann::json jsonCMDLINE = {
        {"type", "CMDLINE"},
        {"executable", "/bin/_binary_does_not_exist"},
        {"check", {{"lookup", "_doesNotExist_"}}}};
    DataAccessor cmdAccessor{jsonCMDLINE};
    EXPECT_NE(cmdAccessor.check(cmdAccessor), true);
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
    bool result = cmdAccessor.check();
    const std::filesystem::path filenamepath = filename;
    std::filesystem::remove(filenamepath);

    EXPECT_EQ(result, true);
}

TEST(DataAccessor, CheckPositiveBitmaskRedefinition)
{
    DataAccessor jAccessor(jsonBITMASK); // {"bitmask", "0x01"}
    PropertyVariant dataValue(0x10f);    // bits 0,1,2,3 and 8
    DataAccessor dAccessor(dataValue);

    // without redefinition
    EXPECT_EQ(jAccessor.check(dAccessor), true); // bit 0
    // with redefinition, bits 1-3 pass
    EXPECT_EQ(jAccessor.check(dAccessor, PropertyVariant(int64_t(0x02))), true);
    EXPECT_EQ(jAccessor.check(dAccessor, PropertyVariant(int64_t(0x04))), true);
    EXPECT_EQ(jAccessor.check(dAccessor, PropertyVariant(int64_t(0x08))), true);
    // bits 4-7 fail
    EXPECT_NE(jAccessor.check(dAccessor, PropertyVariant(int64_t(0x10))), true);
    EXPECT_NE(jAccessor.check(dAccessor, PropertyVariant(int64_t(0x20))), true);
    EXPECT_NE(jAccessor.check(dAccessor, PropertyVariant(int64_t(0x40))), true);
    EXPECT_NE(jAccessor.check(dAccessor, PropertyVariant(int64_t(0x80))), true);
    // finally bit 8 passes
    EXPECT_EQ(jAccessor.check(dAccessor, PropertyVariant(int64_t(256))), true);
}

TEST(DataAccessor, CheckPositiveLookupRedefinition)
{
    const nlohmann::json json = {{"type", "CMDLINE"},
                                 {"executable", "/bin/echo"},
                                 {"arguments", "ff 00 00 00 00 00 02 40 66 28"},
                                 {"check", {{"lookup", "_doesNotExist_"}}}};
    DataAccessor jAccessor(json);

    // not found without redefenition
    EXPECT_NE(jAccessor.check(), true);
    // found with redefinition
    EXPECT_EQ(jAccessor.check(PropertyVariant(std::string{"40 6"})), true);
    EXPECT_EQ(jAccessor.check(PropertyVariant(std::string{"ff 0"})), true);
    EXPECT_EQ(jAccessor.check(PropertyVariant(std::string{"6 28"})), true);
    // redefinition whennot found
    EXPECT_NE(jAccessor.check(PropertyVariant(std::string{"zz"})), true);
}

TEST(DataAccessor, CheckPositiveLookupForDeviceName)
{
    const nlohmann::json json = {{"type", "CMDLINE"},
                                 {"executable", "/bin/echo"},
                                 {"arguments", "query_boot_status [0-7]"},
                                 {"check", {{"lookup", "GPU0"}}}};
    DataAccessor jAccessor(json);
    const std::string device{"GPU0"};
    EXPECT_EQ(jAccessor.check(device), true);
}

TEST(DataAccessor, CheckNegativeLookupForDeviceName)
{
    const nlohmann::json json = {{"type", "CMDLINE"},
                                 {"executable", "/bin/echo"},
                                 {"arguments", "query_boot_status [0-7]"},
                                 {"check", {{"lookup", "GPU0"}}}};
    DataAccessor jAccessor(json);
    const std::string device{"GPU0"};

    PropertyVariant redefineLookup{std::string{"GPU1"}};
    EXPECT_NE(jAccessor.check(device, redefineLookup), true);
}

TEST(DataAccessor, CheckLookupMctpVdmUtilWrapper)
{
    // the wrapper contains the option -dry-run
    // which makes it just print the command line wihout calling mctp-vdm-util
    const nlohmann::json json = {
        {"type", "CMDLINE"},
        {"executable", "mctp-vdm-util-wrapper"},
        {"arguments", "-dry-run query_boot_status [0-7]"},
        {"check", {{"lookup", "-t 20"}}}};

    DataAccessor jAccessor(json);
    const std::string device{"GPU5"};
    EXPECT_EQ(jAccessor.check(device), true);
}

TEST(DataAccessor,  EqualPositiveRegexAgainstNoRegex)
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

TEST(DataAccessor,  EqualNegativeRegexAgainstItself)
{
    const nlohmann::json jsonFile = {
    {"type", "DBUS"},
    {"object", "/xyz/openbmc_project/inventory/system/memory/GPU[0-7]"}};

    DataAccessor jsonAccessor(jsonFile);

    // it is strange to say something is not equal itself
    EXPECT_NE(jsonAccessor == jsonAccessor, true);
}

TEST(DataAccessor,  EqualNegativeNoRegxAgainstRegex)
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

TEST(DataAccessor,  EqualPositiveTypeOnly)
{
    const nlohmann::json jsonFile = {
    {"type", "DBUS"}};

    const nlohmann::json dbusProperty = {
    {"type", "DBUS"}};

    DataAccessor jsonAccessor(jsonFile);
    DataAccessor objectAccessor(dbusProperty);
    EXPECT_EQ(jsonAccessor == objectAccessor, true);
}

TEST(DataAccessor,  EqualPositiveNoRegxAgainstNoRegex)
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

TEST(DataAccessor,  EqualPositiveWithSkippedCheckField)
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

TEST(DataAccessor,  EqualNegativeRegexAgainstNoRegex)
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

TEST(DataAccessor,  EqualNegativeDifferentTypes)
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

TEST(DataAccessor,  EqualNegativeNoTypeInThis)
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

TEST(DataAccessor,  EqualNegativeNoTypeInOther)
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

TEST(DataAccessor,  EqualOperatorNegativeCompleteAccessorDiffObject)
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

TEST(DataAccessor,  EqualOperatorNegativeCompleteAccessorDiffInterface)
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

TEST(DataAccessor,  EqualOperatorNegativeCompleteAccessorDiffProperty)
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
