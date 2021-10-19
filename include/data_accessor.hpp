
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
    std::string longDescription;

  public:
    DataAccessor();
    ~DataAccessor();

  Status getLongDescription(std::string &longDescription);
};

} // namespace data_accessor
