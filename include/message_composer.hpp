
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
    std::string longDescription;

  public:
    MessageComposer();
    ~MessageComposer();

    Status getLongDescription(std::string &longDescription);
};

} // namespace message_composer
