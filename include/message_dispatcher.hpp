
/*
 *
 */

#pragma once

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
    std::string[] longDescription;
  public:
    MessageDispatcher();
    ~MessageDispatcher();

    Status getLongDescription(std::string[]& longDescr);
};

} // namespace message_dispatcher

