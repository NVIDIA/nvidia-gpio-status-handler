
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

  public:
    Logger();
    ~Logger();

};

} // namespace logger
