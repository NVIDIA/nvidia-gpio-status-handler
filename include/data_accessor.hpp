
/*
 *
 */

#pragma once

#include <string>
#include <vector>
namespace data_accessor
{

enum ACCESSOR_TYPE
{
    DBUS = 0,
    OTHER = 1
};

class DataAccessor
{
  public:
    DataAccessor();
    ~DataAccessor();

  private:
    ACCESSOR_TYPE accessorType;
    std::vector<int> accessorValue;
};

} // namespace data_accessor
