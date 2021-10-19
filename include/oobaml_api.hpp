
/*
 *
 */

#pragma once

#include <string>

namespace oobaml_api
{

enum class Status : int
{
  succ,
  error,
  timeout,
};

class OobamlApi
{
  private:
    std::string longDescription;

  public:
    OobamlApi();
    ~OobamlApi();

    Status getLongDescription(std::string &longDescription);
};

} // namespace oobaml_api
