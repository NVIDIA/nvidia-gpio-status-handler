
/*
 *
 */

#pragma once

#include <string>
namespace message_dispatcher
{

enum class Status : int
{
  succ,
  error,
  timeout,
};

class MessageDispatcher
{
  private:

  public:
    MessageDispatcher();
    ~MessageDispatcher();

};

} // namespace message_dispatcher
