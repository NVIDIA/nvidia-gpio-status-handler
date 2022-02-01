
/*
 *
 */

#pragma once

#include <string>

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

  public:
    EventDetection();
    ~EventDetection();

};

} // namespace event_detection
