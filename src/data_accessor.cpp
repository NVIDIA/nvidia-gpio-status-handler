
/*
 *
 */

#include "data_accessor.hpp"

#include <boost/process.hpp>

#include <regex>
#include <string>

namespace data_accessor
{

bool DataAccessor::contains(const DataAccessor& other) const
{
    bool ret = isValid(other._acc);
    if (ret == true)
    {
        for (auto& [key, val] : other._acc.items())
        {
            if (_acc.count(key) == 0)
            {
                std::cout << __PRETTY_FUNCTION__ << "(): "
                          << "key=" << key << " not found in _acc" << std::endl;
                ret = false;
                break;
            }
            // this may have
            auto regex_string = _acc[key].get<std::string>();
            auto val_string = val.get<std::string>();
            const std::regex reg_value{regex_string};
            auto values_match = std::regex_match(val_string, reg_value);
            if (values_match == false)
            {
                std::cout << __PRETTY_FUNCTION__ << "(): "
                          << " values do not match key=" << key
                          << " value=" << val << std::endl;
                ret = false;
                break;
            }
        }
    }
    std::cout << __PRETTY_FUNCTION__ << "(): "
              << "ret=" << ret << std::endl;
    return ret;
}

bool DataAccessor::check(const DataAccessor& otherAcc,
                         const PropertyVariant& redefCriteria,
                         const std::string& device) const
{
    bool ret = true; // defaults to true if accessor["check"] does not exist
    if (_acc.count(checkKey) != 0)
    {
        DataAccessor& accNonConst = const_cast<DataAccessor&>(otherAcc);
        // until this moment device is not necessary in PropertyValue::check()
        accNonConst.read(device);
        if ((ret = accNonConst.hasData()) == true)
        {
            auto checkDefinition = _acc[checkKey].get<CheckDefinitionMap>();
            ret = otherAcc._dataValue->check(checkDefinition, redefCriteria);
        }
    }
    return ret;
}

bool DataAccessor::check(const DataAccessor& otherAcc,
                         const std::string& device,
                         const PropertyVariant& redefCriteria) const
{
    return check(otherAcc, redefCriteria, device);
}

bool DataAccessor::check(const PropertyVariant& redefCriteria,
                         const std::string& device) const
{
    return check(*this, redefCriteria, device);
}

bool DataAccessor::check(const std::string& device,
                         const PropertyVariant& redefCriteria) const
{
    return check(*this, redefCriteria, device);
}

bool DataAccessor::check() const
{
    return check(*this, PropertyVariant(), std::string{""});
}

bool DataAccessor::readDbus()
{
    clearData();
    bool ret = isValidDbusAccessor();
    if (ret == true)
    {
        auto propVariant = readDbusProperty(_acc[objectKey], _acc[interfaceKey],
                                            _acc[propertyKey]);
        ret = setDataValueFromVariant(propVariant);
    }
    std::cout << __PRETTY_FUNCTION__ << "(): "
              << "ret=" << ret << std::endl;
    return ret;
}

bool DataAccessor::runCommandLine(const std::string& device)
{
    clearData();
    bool ret = isValidCmdlineAccessor();
    if (ret == true)
    {
        auto cmd = _acc[executableKey].get<std::string>();
        if (_acc.count(argumentsKey) != 0)
        {
            auto args = _acc[argumentsKey].get<std::string>();
            auto regexPosition = std::string::npos;
            const std::regex reg{".*(\\[[0-9]+\\-[0-9]+\\]).*"};
            std::smatch match;
            if (std::regex_search(args, match, reg))
            {
                std::string regexString = match[1];
                regexPosition = args.find_first_of(regexString);
                // replace the range by device
                if (device.empty() == false)
                {
                    args.replace(regexPosition, regexString.size(), device);
                }
                else // expand the range inside args
                {
                    // TODO: "before A[0-3] after" => "before A0 A1 A2 A3 after"
                    // args = expandRangeFromString(args);
                }
            }
            cmd += ' ' + args;
        }
        std::cout << "[D] " << __PRETTY_FUNCTION__ << "() "
                  << "cmd: " << cmd << std::endl;
        std::string result{""};
        try
        {
            std::string line{""};
            boost::process::ipstream pipe_stream;
            boost::process::child process(cmd, boost::process::std_out >
                                                   pipe_stream);
            while (pipe_stream && std::getline(pipe_stream, line) &&
                   line.empty() == false)
            {
                result += line;
            }
            process.wait();
        }
        catch (const std::exception& error)
        {
            ret = false;
            std::cerr << "[E]"
                      << "Error running the command \'" << cmd << "\' "
                      << error.what() << std::endl;
        }
        // ok lets store the output
        if (ret == true)
        {
            _dataValue =
                std::make_shared<PropertyValue>(PropertyString(result));
        }
    }
    return ret;
}

bool DataAccessor::setDataValueFromVariant(const PropertyVariant& propVariant)
{
    clearData();
    bool ret = propVariant.index() != 0;
    // index not zero, not std::monostate that means valid data
    if (ret == true)
    {
        _dataValue =
            std::make_shared<PropertyValue>(PropertyValue(propVariant));
    }
    return ret;
}

} // namespace data_accessor
