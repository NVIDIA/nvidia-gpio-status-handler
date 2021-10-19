
/*
 *
 */

#pragma once

#include <string>

namespace dat_traverse
{

enum class Status : int
{
  succ,
  error,
  timeout,
};

class DATTraverse
  {
  private:
    std::string longDescription;

  public:
    DATTraverse();
    ~DATTraverse();

    Status getLongDescription(std::string &longDescription);
};

} // namespace dat_traverse
