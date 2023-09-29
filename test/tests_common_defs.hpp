#pragma once

#include "dbus_accessor.hpp"

#include <nlohmann/json.hpp>

class DummyObjectMapper : public dbus::ObjectMapper<DummyObjectMapper>
{

  public:
    std::vector<std::string>
        getSubTreePathsImpl(sdbusplus::bus::bus& bus,
                            const std::string& subtree, int depth,
                            const std::vector<std::string>& interfaces);
};

nlohmann::json event_GPU_VRFailure();
nlohmann::json event_GPU_SpiFlashError();
nlohmann::json testLayersSubDat_GPU_SXM_1();
