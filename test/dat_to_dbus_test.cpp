#include "aml_main.hpp"
#include "dat_traverse.hpp"

#include <nlohmann/json.hpp>

#include <iostream>
#include <set>

#include "gmock/gmock.h"

using namespace nlohmann;
using namespace std;

json getStandardDatJson()
{
    return json::parse(
        "{"
        "\"Baseboard\" : { \"association\" : [\"GPU0\", \"GPU1\", \"GPU2\", \"GPU3\","
        "  \"GPU4\", \"GPU5\", \"GPU6\", \"GPU7\","
        "  \"NVSwitch0\", \"NVSwitch1\", \"NVSwitch2\", \"NVSwitch3\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"GPU0\" : { \"association\" : [\"HSC0\", \"GPU0_ERoT\", \"PCIeRetimer0\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"GPU1\" : { \"association\" : [\"HSC1\", \"GPU1_ERoT\", \"PCIeRetimer1\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"GPU2\" : { \"association\" : [\"HSC2\", \"GPU2_ERoT\", \"PCIeRetimer2\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"GPU3\" : { \"association\" : [\"HSC3\", \"GPU3_ERoT\", \"PCIeRetimer3\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"GPU4\" : { \"association\" : [\"HSC4\", \"GPU4_ERoT\", \"PCIeRetimer4\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"GPU5\" : { \"association\" : [\"HSC5\", \"GPU5_ERoT\", \"PCIeRetimer5\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"GPU6\" : { \"association\" : [\"HSC6\", \"GPU6_ERoT\", \"PCIeRetimer6\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"GPU7\" : { \"association\" : [\"HSC7\", \"GPU7_ERoT\", \"PCIeRetimer7\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"NVSwitch0\" : { \"association\" : ["
        "  \"VR\", \"HSC8\", \"NVSwitch0_ERoT\", \"PCIeSwitch\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"NVSwitch1\" : { \"association\" : ["
        "  \"VR\", \"HSC8\", \"NVSwitch1_ERoT\", \"PCIeSwitch\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"NVSwitch2\" : { \"association\" : ["
        "  \"VR\", \"HSC9\", \"NVSwitch2_ERoT\", \"PCIeSwitch\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"NVSwitch3\" : { \"association\" : ["
        "  \"VR\", \"HSC9\", \"NVSwitch3_ERoT\", \"PCIeSwitch\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"PCIeRetimer0\" : { \"association\" : [\"HSC8\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"PCIeRetimer1\" : { \"association\" : [\"HSC8\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"PCIeRetimer2\" : { \"association\" : [\"HSC8\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"PCIeRetimer3\" : { \"association\" : [\"HSC8\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"PCIeRetimer4\" : { \"association\" : [\"HSC9\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"PCIeRetimer5\" : { \"association\" : [\"HSC9\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"PCIeRetimer6\" : { \"association\" : [\"HSC9\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"PCIeRetimer7\" : { \"association\" : [\"HSC9\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"PCIeSwitch\" : { \"association\" : [\"HSC_STBY\", \"PCIeSwitch_ERoT\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"FPGA\" : { \"association\" : [\"HSC_STBY\", \"FPGA_ERoT\", \"HMC\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"HMC\" : { \"association\" : [\"HSC_STBY\", \"HMC_ERoT\"],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"GPU0_ERoT\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"GPU1_ERoT\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"GPU2_ERoT\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"GPU3_ERoT\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"GPU4_ERoT\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"GPU5_ERoT\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"GPU6_ERoT\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"GPU7_ERoT\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"HSC0\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"HSC1\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"HSC2\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"HSC3\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"HSC4\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"HSC5\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"HSC6\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"HSC7\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"HSC8\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"HSC9\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"NVSwitch0_ERoT\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"NVSwitch1_ERoT\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"NVSwitch2_ERoT\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"NVSwitch3_ERoT\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"PCIeSwitch_ERoT\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"FPGA_ERoT\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"HMC_ERoT\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"HSC_STBY\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] },"
        "\"VR\" : { \"association\" : [],"
        "  \"power_rail\" : [], \"erot_control\" : [], \"pin_status\" : [],"
        "  \"interface_status\" : [], \"protocol_status\" : [],"
        "  \"firmware_status\" : [] }"
        "}");
}

std::map<std::string, dat_traverse::Device> jsonToDeviceMap(json jsonConfig)
{
    std::map<std::string, dat_traverse::Device> result;
    for (const auto& el : jsonConfig.items())
    {
        auto deviceName = el.key();
        dat_traverse::Device device(deviceName, el.value());
        result.insert(
            std::pair<std::string, dat_traverse::Device>(deviceName, device));
    }
    // fill out parents of all child devices on 2nd pass
    for (const auto& entry : result)
    {
        for (const auto& child : entry.second.association)
        {
            result.at(child).parents.push_back(entry.first);
        }
    }
    return result;
}

template <typename T>
void showVector(const std::vector<T>& vec, bool printSize = true,
                bool printIndexes = true)
{
    if (printSize)
    {
        cout << "size = " << vec.size() << endl;
    }
    for (auto i = 0u; i < vec.size(); ++i)
    {
        if (printIndexes)
        {
            std::cout << "[" << i << "]: " << vec.at(i) << std::endl;
        }
        else // ! printIndexes
        {
            std::cout << vec.at(i) << std::endl;
        }
    }
    std::cout << std::endl;
}

template <typename T>
std::vector<T> subVector(const std::vector<T>& target,
                         const std::vector<int>& indexes)
{
    std::vector<T> result;
    for (auto i : indexes)
    {
        result.push_back(target[i]);
    }
    return result;
}

TEST(DatToDbusTest, JsonTest)
{
    std::map<std::string, dat_traverse::Device> devMap =
        jsonToDeviceMap(getStandardDatJson());

    event_handler::DATTraverse datTraverser("");
    datTraverser.setDAT(devMap);

    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("Baseboard"),
              std::vector<std::string>(
                  {"NVSwitch3",    "PCIeSwitch",     "PCIeSwitch_ERoT",
                   "HSC_STBY",     "NVSwitch3_ERoT", "HSC9",
                   "VR",           "NVSwitch2",      "NVSwitch2_ERoT",
                   "NVSwitch1",    "NVSwitch1_ERoT", "HSC8",
                   "NVSwitch0",    "NVSwitch0_ERoT", "GPU7",
                   "PCIeRetimer7", "GPU7_ERoT",      "HSC7",
                   "GPU6",         "PCIeRetimer6",   "GPU6_ERoT",
                   "HSC6",         "GPU5",           "PCIeRetimer5",
                   "GPU5_ERoT",    "HSC5",           "GPU4",
                   "PCIeRetimer4", "GPU4_ERoT",      "HSC4",
                   "GPU3",         "PCIeRetimer3",   "GPU3_ERoT",
                   "HSC3",         "GPU2",           "PCIeRetimer2",
                   "GPU2_ERoT",    "HSC2",           "GPU1",
                   "PCIeRetimer1", "GPU1_ERoT",      "HSC1",
                   "GPU0",         "PCIeRetimer0",   "GPU0_ERoT",
                   "HSC0"}));
    EXPECT_EQ(
        datTraverser.getAssociationConnectedDevices("FPGA"),
        std::vector<std::string>({"HMC", "HMC_ERoT", "HSC_STBY", "FPGA_ERoT"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("FPGA_ERoT"),
              std::vector<std::string>({}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("GPU0"),
              std::vector<std::string>(
                  {"PCIeRetimer0", "HSC8", "GPU0_ERoT", "HSC0"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("GPU0_ERoT"),
              std::vector<std::string>({}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("GPU1"),
              std::vector<std::string>(
                  {"PCIeRetimer1", "HSC8", "GPU1_ERoT", "HSC1"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("GPU1_ERoT"),
              std::vector<std::string>({}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("GPU2"),
              std::vector<std::string>(
                  {"PCIeRetimer2", "HSC8", "GPU2_ERoT", "HSC2"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("GPU2_ERoT"),
              std::vector<std::string>({}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("GPU3"),
              std::vector<std::string>(
                  {"PCIeRetimer3", "HSC8", "GPU3_ERoT", "HSC3"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("GPU3_ERoT"),
              std::vector<std::string>({}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("GPU4"),
              std::vector<std::string>(
                  {"PCIeRetimer4", "HSC9", "GPU4_ERoT", "HSC4"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("GPU4_ERoT"),
              std::vector<std::string>({}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("GPU5"),
              std::vector<std::string>(
                  {"PCIeRetimer5", "HSC9", "GPU5_ERoT", "HSC5"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("GPU5_ERoT"),
              std::vector<std::string>({}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("GPU6"),
              std::vector<std::string>(
                  {"PCIeRetimer6", "HSC9", "GPU6_ERoT", "HSC6"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("GPU6_ERoT"),
              std::vector<std::string>({}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("GPU7"),
              std::vector<std::string>(
                  {"PCIeRetimer7", "HSC9", "GPU7_ERoT", "HSC7"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("GPU7_ERoT"),
              std::vector<std::string>({}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("HMC"),
              std::vector<std::string>({"HMC_ERoT", "HSC_STBY"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("HMC_ERoT"),
              std::vector<std::string>({}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("HSC0"),
              std::vector<std::string>({}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("HSC1"),
              std::vector<std::string>({}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("HSC2"),
              std::vector<std::string>({}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("HSC3"),
              std::vector<std::string>({}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("HSC4"),
              std::vector<std::string>({}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("HSC5"),
              std::vector<std::string>({}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("HSC6"),
              std::vector<std::string>({}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("HSC7"),
              std::vector<std::string>({}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("HSC8"),
              std::vector<std::string>({}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("HSC9"),
              std::vector<std::string>({}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("HSC_STBY"),
              std::vector<std::string>({}));
    EXPECT_EQ(
        datTraverser.getAssociationConnectedDevices("NVSwitch0"),
        std::vector<std::string>({"PCIeSwitch", "PCIeSwitch_ERoT", "HSC_STBY",
                                  "NVSwitch0_ERoT", "HSC8", "VR"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("NVSwitch0_ERoT"),
              std::vector<std::string>({}));
    EXPECT_EQ(
        datTraverser.getAssociationConnectedDevices("NVSwitch1"),
        std::vector<std::string>({"PCIeSwitch", "PCIeSwitch_ERoT", "HSC_STBY",
                                  "NVSwitch1_ERoT", "HSC8", "VR"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("NVSwitch1_ERoT"),
              std::vector<std::string>({}));
    EXPECT_EQ(
        datTraverser.getAssociationConnectedDevices("NVSwitch2"),
        std::vector<std::string>({"PCIeSwitch", "PCIeSwitch_ERoT", "HSC_STBY",
                                  "NVSwitch2_ERoT", "HSC9", "VR"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("NVSwitch2_ERoT"),
              std::vector<std::string>({}));
    EXPECT_EQ(
        datTraverser.getAssociationConnectedDevices("NVSwitch3"),
        std::vector<std::string>({"PCIeSwitch", "PCIeSwitch_ERoT", "HSC_STBY",
                                  "NVSwitch3_ERoT", "HSC9", "VR"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("NVSwitch3_ERoT"),
              std::vector<std::string>({}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("PCIeSwitch"),
              std::vector<std::string>({"PCIeSwitch_ERoT", "HSC_STBY"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("PCIeSwitch_ERoT"),
              std::vector<std::string>({}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("PCIeRetimer0"),
              std::vector<std::string>({"HSC8"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("PCIeRetimer1"),
              std::vector<std::string>({"HSC8"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("PCIeRetimer2"),
              std::vector<std::string>({"HSC8"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("PCIeRetimer3"),
              std::vector<std::string>({"HSC8"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("PCIeRetimer4"),
              std::vector<std::string>({"HSC9"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("PCIeRetimer5"),
              std::vector<std::string>({"HSC9"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("PCIeRetimer6"),
              std::vector<std::string>({"HSC9"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("PCIeRetimer7"),
              std::vector<std::string>({"HSC9"}));
    EXPECT_EQ(datTraverser.getAssociationConnectedDevices("VR"),
              std::vector<std::string>({}));
}
