
/*
 *
 */

#include "data_accessor.hpp"

#include <unistd.h>

#include <boost/process.hpp>

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

bool DataAccessor::check(const DataAccessor& acc) const
{
    bool ret = true; // defaults to true if accessor["check"] does not exist
    if (_acc.count(checkKey) != 0)
    {
        DataAccessor& accNonConst = const_cast<DataAccessor&>(acc);
        accNonConst.read();
        if ((ret = accNonConst.hasData()) == true)
        {
            auto checkDefinition = _acc[checkKey].get<CheckDefinitionMap>();
            ret = acc._dataValue->check(checkDefinition);
        }
    }
    return ret;
}

bool DataAccessor::check() const
{
    return check(*this);
}

bool DataAccessor::readDbus()
{
    bool ret = false;
    clearData();
    if (isValidDbusAccessor() == true)
    {
        auto propVariant = readDbusProperty(_acc[objectKey], _acc[interfaceKey],
                                            _acc[propertyKey]);
        // index not zero, not std::monostate that means valid data
        if ((ret = propVariant.index() != 0) == true)
        {
            _dataValue =
                std::make_shared<PropertyValue>(PropertyValue(propVariant));
        }
    }
    std::cout << __PRETTY_FUNCTION__ << "(): "
              << "ret=" << ret << std::endl;
    return ret;
}

bool DataAccessor::runCommandLine()
{
    clearData();
    bool ret = isValidCmdlineAccessor();
    if (ret == true)
    {
        auto cmd = _acc[executableKey].get<std::string>();
        if (_acc.count(argumentsKey) != 0)
        {
            cmd += ' ' + _acc[argumentsKey].get<std::string>();
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

} // namespace data_accessor
