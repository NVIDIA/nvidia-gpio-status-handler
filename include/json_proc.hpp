#pragma once

#include "device_id.hpp"

#include <nlohmann/json.hpp>

namespace json_proc
{

// JsonPattern ////////////////////////////////////////////////////////////////

class JsonPattern
{
  public:
    explicit JsonPattern(const nlohmann::json& js) : js(js)
    {}

    /**
     * @brief Analogous to DeviceIdPattern::eval, but for json objects instead
     * of strings.
     *
     * TODO: examples
     */
    nlohmann::json eval(const device_id::PatternIndex& index) const;

    /**
     * @brief Generate a sequence of json object in a similar manner as @c
     * DeviceIdPattern::values()
     *
     * The logic is equivalent to converting the json given in the
     * constructor to a raw string, constructing @c DeviceIdPattern of it,
     * calling its @c eval, and parsing every element from the obtained
     * sequence back to a json object.
     */
    std::vector<nlohmann::json> values() const;

  private:
    nlohmann::json js;
};

} // namespace json_proc
