#include "tests_common_defs.hpp"

#include <cassert>

#include "gmock/gmock.h"
using namespace testing;

TEST(ObjectMapper, getAllDevIdObjPaths)
{
    DummyObjectMapper om;
    EXPECT_THAT(
        om.getAllDevIdObjPaths("GPU_SXM_3"),
        UnorderedElementsAre(
            "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_3",
            "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_3/PCIeDevices/GPU_SXM_3",
            "/xyz/openbmc_project/inventory/system/fabrics/HGX_NVLinkFabric_0/Endpoints/GPU_SXM_3",
            "/xyz/openbmc_project/inventory/system/fabrics/HGX_PCIeRetimerTopology_2/Endpoints/GPU_SXM_3",
            "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_3"));
}

TEST(ObjectMapper, getPrimaryDevIdPaths)
{
    DummyObjectMapper om;
    EXPECT_THAT(
        om.getPrimaryDevIdPaths("GPU_SXM_3"),
        UnorderedElementsAre(
            "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_3"));
}

TEST(ObjectMapper, getPrimaryDevIdPaths1)
{
    DummyObjectMapper om;
    EXPECT_THAT(
        om.getPrimaryDevIdPaths("PCIeSwitch_0"),
        UnorderedElementsAre(
            "/xyz/openbmc_project/inventory/system/chassis/HGX_PCIeSwitch_0"));
}
