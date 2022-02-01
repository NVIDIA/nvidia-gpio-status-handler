
/*
 *
 */

#pragma once

#include <string>

namespace message_composer
{

enum class Status : int
{
  succ,
  error,
  timeout,
};

class MessageComposer
{
  private:

  public:
    MessageComposer();
    ~MessageComposer();

};

} // namespace message_composer
