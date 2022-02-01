
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

  public:
    ConfigParser();
    ~ConfigParser();

};

} // namespace config_parser
