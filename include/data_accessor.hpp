
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

  public:
    ACCESSOR_TYPE accessorType;
    std::string accessorMetaData;
};

} // namespace data_accessor
