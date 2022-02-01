
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

  public:
    DATTraverse();
    ~DATTraverse();

};

} // namespace dat_traverse
