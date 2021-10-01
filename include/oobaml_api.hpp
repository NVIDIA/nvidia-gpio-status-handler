
/*
 *
 */

#pragma once

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
    std::string[] longDescription;
  public:
    OobamlApi();
    ~OobamlApi();

    Status getLongDescription(std::string[]& longDescr);
};

} // namespace oobaml_api

