
/*
 *
 */

#pragma once

#include <string>

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
    std::string longDescription;

  public:
    ConfigParser();
    ~ConfigParser();

    Status getLongDescription(std::string &longDescription);
};

} // namespace config_parser
