
/*
 *
 */

#pragma once

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
    std::string[] longDescription;
  public:
    DATTraverse();
    ~DATTraverse();

    Status getLongDescription(std::string[]& longDescr);
};

} // namespace dat_traverse

