#include <gpio_json_config.hpp>
#include <gpio_utils.hpp>
#include <phosphor-logging/log.hpp>

#include <fstream>
#include <sstream>

using namespace std;
using json = nlohmann::json;

using phosphor::logging::entry;
using phosphor::logging::level;
using phosphor::logging::log;

namespace gpio_handler
{

// Adding a custom layer of json parsing exception types, in analogy to those
// listed at https://json.nlohmann.me/home/exceptions/
static constexpr int jsonSemanticError = 600;

const string GpioJsonConfig::configKeyGpioChip = "gpio_chip";
const string GpioJsonConfig::configKeyGpioPin = "gpio_pin";
const string GpioJsonConfig::configKeyInitialPinVal = "initial";
const string GpioJsonConfig::configKeyReadPeriod = "read_period_sec";

const string GpioJsonConfig::expectedJsonConfigFormat =
    string("{\n") +                                                       //
    string("  \"I2C3_PIN\" : {\n") +                                      //
    string("    \"") + configKeyGpioChip + string("\" : 0,\n") +          //
    string("    \"") + configKeyGpioPin + string("\" : 108,\n") +         //
    string("    \"") + configKeyInitialPinVal + string("\" : false,\n") + //
    string("    \"") + configKeyReadPeriod + string("\" : 0.2\n") +       //
    string("  },\n") +                                                    //
    string("  \"I2C4_PIN\" : {\n") +                                      //
    string("    \"") + configKeyGpioChip + string("\" : 1,\n") +          //
    string("    \"") + configKeyGpioPin + string("\" : 32,\n") +          //
    string("    \"") + configKeyInitialPinVal + string("\" : false,\n") + //
    string("    \"") + configKeyReadPeriod + string("\" : 3\n") +         //
    string("  }\n") +                                                     //
    string("  ...\n") +                                                   //
    string("}\n");                                                        //

const nlohmann::json& GpioJsonConfig::getConfig() const
{
    return config;
}

static string jsonTypeToString(json::value_t type)
{
    switch (type)
    {
        case json::value_t::null:
            return "null";
        case json::value_t::boolean:
            return "boolean";
        case json::value_t::string:
            return "string";
        case json::value_t::number_integer:
            return "number (integer)";
        case json::value_t::number_unsigned:
            return "number (unsigned integer)";
        case json::value_t::number_float:
            return "number (floating-point)";
        case json::value_t::object:
            return "object";
        case json::value_t::array:
            return "array";
        case json::value_t::binary:
            return "binary";
        case json::value_t::discarded:
            return "discarded";
        default:
            return "<unknown>";
    }
}

static void logErrorBadType(const string& typeName, const json& jsonValue,
                            const string& context)
{
    stringstream ss;
    ss << "Error when parsing value of the attribute '" << context
       << "': expected value of type '" << typeName << "'. Got '"
       << jsonValue.dump(2) << "' (type: " << jsonTypeToString(jsonValue.type())
       << ")" << endl;
    log<level::ERR>(ss.str().c_str());
}

static bool isJsonValueObject(const json& jsonValue, const string& context)
{
    bool result = jsonValue.is_object();
    if (!result)
    {
        logErrorBadType("object", jsonValue, context);
    }
    return result;
}

static bool isJsonValueBoolean(const json& jsonValue, const string& context)
{
    bool result = jsonValue.is_boolean();
    if (!result)
    {
        logErrorBadType("boolean", jsonValue, context);
    }
    return result;
}

static bool isJsonValueUnsignedInt(const json& jsonValue, const string& context)
{
    bool result = jsonValue.is_number_unsigned();
    if (!result)
    {
        logErrorBadType("unsigned int", jsonValue, context);
    }
    return result;
}

static bool isJsonValuePositiveNumber(const json& jsonValue,
                                      const string& context)
{
    bool result = jsonValue.is_number() && ((float)jsonValue) > 0;
    if (!result)
    {
        logErrorBadType("positive number", jsonValue, context);
    }
    return result;
}

static bool hasJsonObjectAttr(const json& jsonObject, const string& attrName)
{
    bool result = jsonObject.contains(attrName);
    if (!result)
    {
        stringstream ss;
        ss << "No expected '" << attrName
           << "' attribute found in the json object '" << jsonObject.dump(2)
           << "'" << endl;
        log<level::ERR>(ss.str().c_str());
    }
    return result;
}

static bool isJsonPropertyGood(const json& jsonObject, const string& attrName,
                               bool (*valueCriterion)(const json&,
                                                      const string&))
{
    return hasJsonObjectAttr(jsonObject, attrName) &&
           valueCriterion(jsonObject[attrName], attrName);
}

static bool isPinDescriptionGood(const string& pinName, const json& jsonValue)
{
    return isJsonValueObject(jsonValue, pinName) &&
           isJsonPropertyGood(jsonValue, GpioJsonConfig::configKeyGpioChip,
                              isJsonValueUnsignedInt) &&
           isJsonPropertyGood(jsonValue, GpioJsonConfig::configKeyGpioPin,
                              isJsonValueUnsignedInt) &&
           isJsonPropertyGood(jsonValue, GpioJsonConfig::configKeyInitialPinVal,
                              isJsonValueBoolean) &&
           isJsonPropertyGood(jsonValue, GpioJsonConfig::configKeyReadPeriod,
                              isJsonValuePositiveNumber);
}

static bool isGpioNameGood(const string& name)
{
    // Gpio names can containg only alphanumeric values
    bool isSizeGood = name.size() > 0;
    if (!isSizeGood)
    {
        log<level::ERR>("Empty gpio pin name", pinNameEntry(""));
    }
    bool isNameGood = isSizeGood;
    for (auto i = 0u; i < name.size() && isNameGood; ++i)
    {
        bool isCharGood =
            std::isalnum(name[i], std::locale::classic()) || name[i] == '_';
        if (!isCharGood)
        {
            stringstream ss;
            ss << "Gpio name '" << name << "' contains invalid character: '"
               << name[i] << "'" << endl;
            ss << "Allowed characters: basic alphanumeric + '_' ";
            log<level::ERR>(ss.str().c_str(), pinNameEntry(name));
        }
        isNameGood = isCharGood;
    }
    return isNameGood;
}

static bool isJsonObjectNonEmpty(const json& jsonObject)
{
    bool isNonEmpty = !jsonObject.empty();
    if (!isNonEmpty)
    {
        log<level::ERR>("Empty json object");
    }
    return isNonEmpty;
}

static bool isJsonConfigFormatGood(const json& jsonValue)
{
    bool isFormatGood = isJsonValueObject(jsonValue, "<root>") &&
                        isJsonObjectNonEmpty(jsonValue);
    for (auto it = jsonValue.cbegin(); it != jsonValue.cend() && isFormatGood;
         ++it)
    {
        isFormatGood = isGpioNameGood(it.key()) &&
                       isPinDescriptionGood(it.key(), it.value());
    }
    return isFormatGood;
}

GpioConfigError::GpioConfigError(const char* message) :
    json::exception(jsonSemanticError, message)
{}

GpioJsonConfig::GpioJsonConfig(const string& fileName)
{
#ifdef ENABLE_GSH_LOGS
    {
        stringstream ss;
        ss << "Reading json file '" << fileName << "'";
        log<level::INFO>(ss.str().c_str(), entry("FILE=%s", fileName.c_str()));
    }
#endif

    ifstream jsonFile(fileName);
    if (jsonFile.good())
    {
        json jsonConfig;
        jsonFile >> jsonConfig;

        if (isJsonConfigFormatGood(jsonConfig))
        {
            config = jsonConfig;
        }
        else
        {
            throw GpioConfigError("Malformed config file");
        }
    }
    else
    {
        stringstream ss;
        ss << "Error when reading the file '" << fileName << "'" << endl
           << "State bits: " << endl
           << " good = " << jsonFile.good() << endl
           << " eof  = " << jsonFile.eof() << endl
           << " fail = " << jsonFile.fail() << endl
           << " bad  = " << jsonFile.bad();
        log<level::ERR>(ss.str().c_str(), entry("FILE=%s", fileName.c_str()));
        throw GpioConfigError(ss.str().c_str());
    }
}

} // namespace gpio_handler
