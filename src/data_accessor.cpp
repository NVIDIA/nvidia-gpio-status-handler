
/*
 *
 */

#include "data_accessor.hpp"

namespace data_accessor
{

DataAccessor::DataAccessor()
{
}

DataAccessor::~DataAccessor()
{
}

Status DataAccessor::getLongDescription(std::string &longDescription)
{
    return Status::succ;
}

} // namespace data_accessor
