#include "tests_common_defs.hpp"

using namespace nlohmann;

// Autentic event definition (openbmc, commit
// 36b4bec02bb27c6d5146d4776be8f922e6102194)
json event_GPU_VRFailure()
{
    return json{
        {"event", "VR Failure"},
        {"device_type", "GPU_SXM_[1-8]"},
        {"category", {"power_rail"}},
        {"event_trigger",
         {{"type", "DBUS"},
          {"object", "/xyz/openbmc_project/GpioStatusHandler"},
          {"interface", "xyz.openbmc_project.GpioStatus"},
          {"property", "I2C3_ALERT"},
          {"check", {{"equal", "false"}}}}},
        {"accessor",
         {{"type", "DeviceCoreAPI"},
          {"device_id", "range"},
          {"property", "gpu.interrupt.powerGoodAbnormalChange"},
          {"check", {{"equal", "1"}}}}},
        {"severity", "Critical"},
        {"resolution", "Power cycle the BaseBoard. If the problem persists, "
                       "isolate the server for RMA evaluation."},
        {"trigger_count", 0},
        {"event_counter_reset", {{"type", ""}, {"metadata", ""}}},
        {"redfish",
         {{"message_id", "ResourceEvent.1.0.ResourceErrorsDetected"},
          {"message_args",
           {{"patterns", {"{GPUId} PowerGood", "Abnormal Power Change"}},
            {"parameters",
             {{{"type", "DIRECT"}, {"field", "CurrentDeviceName"}}}}}}}},
        {"telemetries",
         {{{"name", "GPU Power Good Abnormal change"},
           {"type", "DeviceCoreAPI"},
           {"property", "gpu.interrupt.powerGoodAbnormalChange"}}}},
        {"action", ""},
        {"value_as_count", false}};
}

// Autentic event definition (openbmc, commit
// 36b4bec02bb27c6d5146d4776be8f922e6102194)
json event_GPU_SpiFlashError()
{
    return json{
        {"event", "SPI flash error"},
        {"device_type", "GPU_SXM_[1-8]"},
        {"event_trigger",
         {{"type", "DBUS"},
          {"object",
           "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_[1-8]"},
          {"interface",
           "xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig"},
          {"property", "MinSpeed"}}},
        {"accessor",
         {{"type", "CMDLINE"},
          {"executable", "mctp-vdm-util-wrapper"},
          {"arguments", "AP0_SPI_READ_FAILURE GPU_SXM_[1-8]"},
          {"check", {{"equal", "1"}}}}},
        {"severity", "Critical"},
        {"resolution",
         "Cordon HGX server from the cluster for RMA evaluation."},
        {"trigger_count", 0},
        {"event_counter_reset", {{"type", ""}, {"metadata", ""}}},
        {"redfish",
         {{"message_id", "ResourceEvent.1.0.ResourceErrorsDetected"},
          {"message_args",
           {{"patterns", {"{GPUId} Firmware", "SPI Flash Error"}},
            {"parameters",
             {{{"type", "DIRECT"}, {"field", "CurrentDeviceName"}}}}}}}},
        {"telemetries", {}},
        {"action", ""},
        {"value_as_count", false}};
}

// Authentic sub-DAT (for associations defined by test layers) of the
// 'GPU_SXM_1' (openbmc, commit 24d1661e7b47fc43b36a9785abe3ab264bbd2de6)
json testLayersSubDat_GPU_SXM_1()
{
    json js;
    js["ERoT_GPU_SXM_1"]["association"] = {"StandbyHSC_0"};
    js["ERoT_GPU_SXM_1"]["power_rail"] = {
        {{"name", "StandbyHSC_0"},
         {"accessor", {{"type", "DEVICE"}, {"device_name", "StandbyHSC_0"}}},
         {"expected_value", "PASS"}}};
    js["ERoT_GPU_SXM_1"]["pin_status"] = {
        {{"name", "Fatal Error Status"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "fpga_regtbl_wrapper"},
           {"arguments", "GPU1_EROT_FATAL_ERROR_N GPU_SXM_1"}}},
         {"expected_value", "1"}},
        {{"name", "Recovery Status"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "fpga_regtbl_wrapper"},
           {"arguments", "FPGA_SXM0_EROT_RECOV_L GPU_SXM_1"}}},
         {"expected_value", "1"}}};
    js["ERoT_GPU_SXM_1"]["erot_control"] = json::array();
    js["ERoT_GPU_SXM_1"]["interface_status"] = json::array();
    js["ERoT_GPU_SXM_1"]["protocol_status"] = json::array();
    js["ERoT_GPU_SXM_1"]["firmware_status"] = {
        {{"name", "Authentication Status"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "mctp-vdm-util-wrapper"},
           {"arguments", "EC_TAG0_AUTH_ERROR GPU_SXM_1"}}},
         {"expected_value", "0"}},
        {{"name", "Firmware"},
         {"accessor",
          {{"type", "DBUS"},
           {"object", "/xyz/openbmc_project/software/HGX_FW_ERoT_GPU_SXM_1"},
           {"interface", "xyz.openbmc_project.Software.Version"},
           {"property", "Version"}}},
         {"expected_value", ""}}};
    js["ERoT_GPU_SXM_1"]["data_dump"] = {
        {{"name", "Boot status"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "mctp-vdm-util-wrapper"},
           {"arguments", "query_boot_status GPU_SXM_1"}}},
         {"expected_value", ""}},
        {{"name", "ERoT selftest log"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "mctp-vdm-util-wrapper"},
           {"arguments", "\"selftest 08 00 00 00\" GPU_SXM_1"}}},
         {"expected_value", ""}}};
    js["PCIeRetimer_0"]["association"] = {"HSC_8"};
    js["PCIeRetimer_0"]["power_rail"] = {
        {{"name", "HSC_8"},
         {"accessor", {{"type", "DEVICE"}, {"device_name", "HSC_8"}}},
         {"expected_value", "PASS"}}};
    js["PCIeRetimer_0"]["pin_status"] = {
        {{"name", "PGOOD Status"},
         {"accessor",
          {{"type", "DBUS"},
           {"object",
            "/xyz/openbmc_project/inventory/system/chassis/HGX_PCIeRetimer_0"},
           {"interface", "xyz.openbmc_project.State.Chassis"},
           {"property", "CurrentPowerState"}}},
         {"expected_value", "xyz.openbmc_project.State.Chassis.PowerState.On"}},
        {{"name", "0V9 Power Enablement"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "fpga_regtbl_wrapper"},
           {"arguments", "FPGA_RET_0123_0V9_PWR_EN PCIeRetimer_0"}}},
         {"expected_value", "1"}},
        {{"name", "0V9 PGOOD Status"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "fpga_regtbl_wrapper"},
           {"arguments", "RET0_0123_0V9_PG PCIeRetimer_0"}}},
         {"expected_value", "1"}},
        {{"name", "0V9 PGOOD Interrupt"},
         {"accessor",
          {{"type", "DeviceCoreAPI"},
           {"property", "pcieretimer.0V9.abnormalPowerChange"}}},
         {"expected_value", "0"}},
        {{"name", "1V8LDO Power Enablement"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "fpga_regtbl_wrapper"},
           {"arguments", "FPGA_RET_0123_1V8LDO_PWR_EN PCIeRetimer_0"}}},
         {"expected_value", "1"}},
        {{"name", "1V8LDO PGOOD Status"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "fpga_regtbl_wrapper"},
           {"arguments", "RET_0123_1V8LDO_PG PCIeRetimer_0"}}},
         {"expected_value", "1"}},
        {{"name", "1V8LDO PGOOD Interrupt"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "fpga_regtbl_wrapper"},
           {"arguments", "RET_0123_1V8LDO_INT_PG PCIeRetimer_0"}}},
         {"expected_value", "0"}},
        {{"name", "1V8VDD Power Enablement"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "fpga_regtbl_wrapper"},
           {"arguments", "FPGA_RET_0123_1V8VDD_PWR_EN PCIeRetimer_0"}}},
         {"expected_value", "1"}},
        {{"name", "1V8VDD PGOOD Status"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "fpga_regtbl_wrapper"},
           {"arguments", "RET_0123_1V8VDD_PG PCIeRetimer_0"}}},
         {"expected_value", "1"}},
        {{"name", "1V8VDD PGOOD Interrupt"},
         {"accessor",
          {{"type", "DeviceCoreAPI"},
           {"property", "pcieretimer.1V8VDD.abnormalPowerChange"}}},
         {"expected_value", "0"}}};
    js["PCIeRetimer_0"]["erot_control"] = json::array();
    js["PCIeRetimer_0"]["interface_status"] = {
        {{"name", "Clock Enablement"},
         {"accessor",
          {{"type", "DeviceCoreAPI"},
           {"property", "pcieretimer.pcie.clockOutputEnableState"}}},
         {"expected_value", "1"}},
        {{"name", "PCIe AER Status non-fatal error count"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "pcie_wrapper"},
           {"arguments", "pcie_non_fatal_error_count PCIeRetimer_0"}}},
         {"expected_value", "0"}},
        {{"name", "PCIe AER Status fatal error count"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "pcie_wrapper"},
           {"arguments", "pcie_fatal_error_count PCIeRetimer_0"}}},
         {"expected_value", "0"}}};
    js["PCIeRetimer_0"]["protocol_status"] = json::array();
    js["PCIeRetimer_0"]["firmware_status"] = {
        {{"name", "Firmware"},
         {"accessor",
          {{"type", "DBUS"},
           {"object", "/xyz/openbmc_project/software/HGX_FW_PCIeRetimer_0"},
           {"interface", "xyz.openbmc_project.Software.Version"},
           {"property", "Version"}}},
         {"expected_value", ""}},
        {{"name", "WriteProtect Status"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "nvlink-wp-status-wrapper"},
           {"arguments", "pcieretimer_eeprom_wp_status PCIeRetimer_0"}}},
         {"expected_value", ""}}};
    js["PCIeRetimer_0"]["data_dump"] = json::array();
    js["StandbyHSC_0"]["association"] = json::array();
    js["StandbyHSC_0"]["power_rail"] = json::array();
    js["StandbyHSC_0"]["pin_status"] = {
        {{"name", "PGOOD Status"},
         {"accessor",
          {{"type", "DeviceCoreAPI"},
           {"property", "hsc.powerSupply.enablement"}}},
         {"expected_value", "1"}},
        {{"name", "PGOOD Interrupt"},
         {"accessor",
          {{"type", "DeviceCoreAPI"},
           {"property", "hsc.power.abnormalPowerChange"}}},
         {"expected_value", "0"}},
        {{"name", "Alert Status"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "fpga_regtbl_wrapper"},
           {"arguments", "HSC_STBY_ALERT_N StandbyHSC_0"}}},
         {"expected_value", "1"}},
        {{"name", "Alert Interrupt"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "fpga_regtbl_wrapper"},
           {"arguments", "HSC_STBY_ALERT_INT_N StandbyHSC_0"}}},
         {"expected_value", "0"}}};
    js["StandbyHSC_0"]["erot_control"] = json::array();
    js["StandbyHSC_0"]["interface_status"] = json::array();
    js["StandbyHSC_0"]["protocol_status"] = json::array();
    js["StandbyHSC_0"]["firmware_status"] = json::array();
    js["StandbyHSC_0"]["data_dump"] = json::array();
    js["HSC_0"]["association"] = json::array();
    js["HSC_0"]["power_rail"] = json::array();
    js["HSC_0"]["pin_status"] = {
        {{"name", "Power Enablement (left)"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "fpga_regtbl_wrapper"},
           {"arguments", "FPGA_HSC1_LEFT_EN HSC_0"}}},
         {"expected_value", "1"}},
        {{"name", "PGOOD Status"},
         {"accessor",
          {{"type", "DeviceCoreAPI"},
           {"property", "hsc.powerSupply.enablement"}}},
         {"expected_value", "1"}},
        {{"name", "PGOOD Interrupt"},
         {"accessor",
          {{"type", "DeviceCoreAPI"},
           {"property", "hsc.power.abnormalPowerChange"}}},
         {"expected_value", "0"}},
        {{"name", "Alert Status"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "fpga_regtbl_wrapper"},
           {"arguments", "HSC1_ALERT_N HSC_0"}}},
         {"expected_value", "1"}},
        {{"name", "Alert Interrupt"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "fpga_regtbl_wrapper"},
           {"arguments", "HSC1_ALERT_INT_N HSC_0"}}},
         {"expected_value", "0"}}};
    js["HSC_0"]["erot_control"] = json::array();
    js["HSC_0"]["interface_status"] = json::array();
    js["HSC_0"]["protocol_status"] = json::array();
    js["HSC_0"]["firmware_status"] = json::array();
    js["HSC_0"]["data_dump"] = json::array();
    js["HSC_8"]["association"] = json::array();
    js["HSC_8"]["power_rail"] = json::array();
    js["HSC_8"]["pin_status"] = {
        {{"name", "Power Enablement (left)"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "fpga_regtbl_wrapper"},
           {"arguments", "FPGA_HSC9_LEFT_EN HSC_8"}}},
         {"expected_value", "1"}},
        {{"name", "PGOOD Status"},
         {"accessor",
          {{"type", "DeviceCoreAPI"},
           {"property", "hsc.powerSupply.enablement"}}},
         {"expected_value", "1"}},
        {{"name", "PGOOD Interrupt"},
         {"accessor",
          {{"type", "DeviceCoreAPI"},
           {"property", "hsc.power.abnormalPowerChange"}}},
         {"expected_value", "0"}},
        {{"name", "Alert Status"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "fpga_regtbl_wrapper"},
           {"arguments", "HSC9_ALERT_N HSC_8"}}},
         {"expected_value", "1"}},
        {{"name", "Alert Interrupt"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "fpga_regtbl_wrapper"},
           {"arguments", "HSC9_ALERT_INT_N HSC_8"}}},
         {"expected_value", "0"}}};
    js["HSC_8"]["erot_control"] = json::array();
    js["HSC_8"]["interface_status"] = json::array();
    js["HSC_8"]["protocol_status"] = json::array();
    js["HSC_8"]["firmware_status"] = json::array();
    js["HSC_8"]["data_dump"] = json::array();
    js["GPU_SXM_1"]["association"] = {"ERoT_GPU_SXM_1", "PCIeRetimer_0",
                                      "HSC_0"};
    js["GPU_SXM_1"]["power_rail"] = {
        {{"name", "HSC_0"},
         {"accessor", {{"type", "DEVICE"}, {"device_name", "HSC_0"}}},
         {"expected_value", "PASS"}}};
    js["GPU_SXM_1"]["pin_status"] = {
        {{"name", "Power Enablement"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "fpga_regtbl_wrapper"},
           {"arguments", "FPGA_SXM0_PWR_EN GPU_SXM_1"}}},
         {"expected_value", "1"}},
        {{"name", "PGOOD Status From GPU"},
         {"accessor",
          {{"type", "DBUS"},
           {"object",
            "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_1"},
           {"interface", "xyz.openbmc_project.State.Chassis"},
           {"property", "CurrentPowerState"}}},
         {"expected_value", "xyz.openbmc_project.State.Chassis.PowerState.On"}},
        {{"name", "PGOOD Status"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "fpga_regtbl_wrapper"},
           {"arguments", "GPU1_PWRGD GPU_SXM_1"}}},
         {"expected_value", "1"}},
        {{"name", "PGOOD Interrupt"},
         {"accessor",
          {{"type", "DeviceCoreAPI"},
           {"property", "gpu.interrupt.powerGoodAbnormalChange"}}},
         {"expected_value", "0"}}};
    js["GPU_SXM_1"]["erot_control"] = {
        {{"name", "ERoT_GPU_SXM_1"},
         {"accessor", {{"type", "DEVICE"}, {"device_name", "ERoT_GPU_SXM_1"}}},
         {"expected_value", "PASS"}}};
    js["GPU_SXM_1"]["interface_status"] = {
        {{"name", "I2CDirect Status"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "i2c_wrapper"},
           {"arguments", "i2c_access GPU_SXM_1"}}},
         {"expected_value", "link-up"}},
        {{"name", "SPI Flash"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "mctp-vdm-util-wrapper"},
           {"arguments", "AP0_SPI_READ_FAILURE GPU_SXM_1"}}},
         {"expected_value", "0"}},
        {{"name", "Clock Enablement"},
         {"accessor",
          {{"type", "DeviceCoreAPI"},
           {"property", "gpu.pcie.clockOutputEnableState"}}},
         {"expected_value", "1"}},
        {{"name", "PCIe AER Status - non-fatal error count"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "pcie_wrapper"},
           {"arguments", "pcie_non_fatal_error_count GPU_SXM_1"}}},
         {"expected_value", "0"}},
        {{"name", "PCIe AER Status - fatal error count"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "pcie_wrapper"},
           {"arguments", "pcie_fatal_error_count GPU_SXM_1"}}},
         {"expected_value", "0"}},
        {{"name", "PCIeRetimer_0"},
         {"accessor", {{"type", "DEVICE"}, {"device_name", "PCIeRetimer_0"}}},
         {"expected_value", "PASS"}},
        {{"name", "NVLink Clock Enablement"},
         {"accessor",
          {{"type", "DeviceCoreAPI"},
           {"property", "gpu.nvlink.clockOutputEnableState"}}},
         {"expected_value", "1"}}};
    js["GPU_SXM_1"]["protocol_status"] = {
        {{"name", "I2CDirect Status"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "i2c_wrapper"},
           {"arguments", "i2c_access GPU_SXM_1"}}},
         {"expected_value", "link-up"}}};
    js["GPU_SXM_1"]["firmware_status"] = {
        {{"name", "Authentication Status"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "mctp-vdm-util-wrapper"},
           {"arguments", "active_auth_status GPU_SXM_1"}}},
         {"expected_value", "1"}},
        {{"name", "Boot Completion Status"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "mctp-vdm-util-wrapper"},
           {"arguments", "AP0_BOOTCOMPLETE_TIMEOUT GPU_SXM_1"}}},
         {"expected_value", "0"}},
        {{"name", "Heartbeat Status"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "mctp-vdm-util-wrapper"},
           {"arguments", "AP0_HEARTBEAT_TIMEOUT GPU_SXM_1"}}},
         {"expected_value", "0"}},
        {{"name", "Firmware"},
         {"accessor",
          {{"type", "DBUS"},
           {"object", "/xyz/openbmc_project/software/HGX_FW_GPU_SXM_1"},
           {"interface", "xyz.openbmc_project.Software.Version"},
           {"property", "Version"}}},
         {"expected_value", ""}},
        {{"name", "InfoROM"},
         {"accessor",
          {{"type", "DBUS"},
           {"object", "/xyz/openbmc_project/software/HGX_InfoROM_GPU_SXM_1"},
           {"interface", "xyz.openbmc_project.Software.Version"},
           {"property", "Version"}}},
         {"expected_value", ""}},
        {{"name", "WriteProtect Status"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "nvlink-wp-status-wrapper"},
           {"arguments", "gpu_spiflash_wp_status GPU_SXM_1"}}},
         {"expected_value", ""}}};
    js["GPU_SXM_1"]["data_dump"] = {
        {{"name", "Boot status"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "mctp-vdm-util-wrapper"},
           {"arguments", "query_boot_status GPU_SXM_1"}}},
         {"expected_value", ""}},
        {{"name", "XID event"},
         {"accessor",
          {{"type", "CMDLINE"},
           {"executable", "echo"},
           {"arguments",
            "To check captured XIDs, Please check URI /redfish/v1/Chassis/HGX_GPU_SXM_1/LogServices/XID/Entries"}}},
         {"expected_value", ""}}};
    return js;
}
