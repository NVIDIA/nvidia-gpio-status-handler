
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
  public:
    OobamlApi();
    ~OobamlApi();
};

} // namespace oobaml_api
