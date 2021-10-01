
/*
 *
 */

#pragma once

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
    std::string[] longDescription;
  public:
    EventHandler();
    ~EventHandler();

    Status getLongDescription(std::string[]& longDescr);
};

} // namespace event_handler

