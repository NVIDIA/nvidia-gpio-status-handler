
/*
 *
 */

#pragma once

namespace event_detection
{

enum class Status : int
{
    succ,
    error,
    timeout,
};

class EventDetection
{
  private:
    std::string[] longDescription;
  public:
    EventDetection();
    ~EventDetection();

    Status getLongDescription(std::string[]& longDescr);
};

} // namespace event_detection

