
/*
 *
 */

#pragma once

namespace config_parser
{

enum class Status : int
{
    succ,
    error,
    timeout,
};

class ConfigParser
{
  private:
    std::string[] longDescription;
  public:
    ConfigParser();
    ~ConfigParser();

    Status getLongDescription(std::string[]& longDescr);
};

} // namespace config_parser

