
/*
 *
 */

#pragma once

#include <string>

namespace data_accessor
{

enum class Status : int
{
  succ,
  error,
  timeout,
};

class DataAccessor
{
  private:

  public:
    DataAccessor();
    ~DataAccessor();

};

} // namespace data_accessor
