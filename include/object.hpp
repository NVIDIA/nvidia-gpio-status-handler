
/**
 *
 */

#pragma once

#include <string>
namespace object
{

/**
 * @brief Base class for all in AML
 */
class Object
{
  public:
    Object();
    ~Object();

  public:
    std::string& getName(void)
    {
        return _name;
    }

    void setName(const std::string& name)
    {
        _name = name;
    }

  private:
    /**
     * @brief Obejct name.
     */
    std::string _name;
};

} // namespace object
