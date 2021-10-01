
/*
 *
 */

#pragma once

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
    std::string[] longDescription;
  public:
    Logger();
    ~Logger();

    Status getLongDescription(std::string[]& longDescr);
};

} // namespace logger

