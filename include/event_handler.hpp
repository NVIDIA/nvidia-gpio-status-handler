
/*
 *
 */

#pragma once

#include <string>

namespace event_handler
{

enum class Status : int
{
  succ,
  error,
  timeout,
};

class EventHandler
{
  private:
    std::string longDescription;

  public:
    EventHandler();
    ~EventHandler();

    Status getLongDescription(std::string &longDescription);
};

} // namespace event_handler
