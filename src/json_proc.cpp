#include "json_proc.hpp"

#include "data_accessor.hpp"

namespace json_proc
{

// JsonPattern ////////////////////////////////////////////////////////////////

static void evalJson(const device_id::PatternIndex& index,
                     const nlohmann::json& jsonPattern, nlohmann::json& result)
{
    if (jsonPattern.is_string())
    {
        result = device_id::DeviceIdPattern(jsonPattern.get<std::string>())
                     .eval(index);
    }
    else if (jsonPattern.is_object())
    {
        for (const auto& [key, value] : jsonPattern.items())
        {
            evalJson(index, jsonPattern[key], result[key]);
        }
    }
    else if (jsonPattern.is_array())
    {
        for (unsigned i = 0; i < jsonPattern.size(); ++i)
        {
            evalJson(index, jsonPattern[i], result[i]);
        }
    }
    else
    {
        result = jsonPattern;
    }
}

nlohmann::json JsonPattern::eval(const device_id::PatternIndex& index) const
{
    nlohmann::json result = this->js;
    evalJson(index, this->js, result);
    return result;
}

} // namespace json_proc
