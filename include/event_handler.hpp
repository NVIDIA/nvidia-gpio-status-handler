
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

  public:
    EventHandler();
    ~EventHandler();

};

} // namespace event_handler
