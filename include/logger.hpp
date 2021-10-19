
/*
 *
 */

#pragma once

#include <string>
namespace logger
{

enum class Status : int
{
  succ,
  error,
  timeout,
};

class Logger
{
  private:
    std::string longDescription;

  public:
    Logger();
    ~Logger();

    Status getLongDescription(std::string &longDescription);
};

} // namespace logger
