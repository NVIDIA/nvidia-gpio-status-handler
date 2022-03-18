
/*
 *
 */

#pragma once

#include <nlohmann/json.hpp>

#include <iostream>
#include <map>
#include <regex>
#include <string>
#include <vector>

namespace data_accessor
{
constexpr auto typeKey = "type";
constexpr auto nameKey = "name";

static std::map<std::string, std::vector<std::string>> accessorTypeKeys = {
    {"DBUS", {"object", "interface", "property"}},
    {"OTHER", {"other"}},
};

/**
 * @brief A class for Data Accessor
 *
 */
class DataAccessor
{

  public:
    DataAccessor()
    {}

    DataAccessor(const nlohmann::json& acc) : _acc(acc)
    {
        std::cout << "Const.: _acc: " << _acc << "\n";
    }

    ~DataAccessor() = default;

  public:
    /**
     * @brief Assign json data.
     *
     * @param acc
     * @return nlohmann::json&
     */
    nlohmann::json& operator=(const nlohmann::json& acc)
    {
        if (!isValid(acc))
        {
            std::cout << "not valid: acc = " << acc << "\n";
            return _acc;
        }

        _acc = acc;
        return _acc;
    }

    /**
     * @brief Comparison operator of the accessor
     *
     * @param other
     * @return true
     * @return false
     */
    bool operator==(const DataAccessor& other)
    {
        std::cout << "This: " << this->_acc << ", Other: " << other._acc
                  << "\n";
        if (!isValid(other._acc))
        {
            return false;
        }
        if (this->_acc[typeKey] != other._acc[typeKey])
        {
            return false;
        }
        for (auto& [key, val] : _acc.items())
        {
            std::cout << "key: " << key << ", val: " << val << "\n";
            if (key == typeKey)
            {
                continue;
            }
            else
            {
                if (other._acc.count(key) == 0)
                {
                    // the key is not in 'other'
                    std::cout << "Other has no key. False.\n";
                    return false;
                }

                const std::regex r{val}; // 'val' could have regex format
                return std::regex_match(other._acc[key].get<std::string>(), r);
            }
        }
        return this->_acc == other._acc;
    }

    /**
     * @brief Access accessor just like do it on json
     *
     * @param key
     * @return auto
     */
    auto operator[](const std::string& key)
    {
        return _acc[key];
    }

    /**
     * @brief Output the accessor details
     *
     * @param os
     * @param acc
     * @return std::ostream&
     */
    friend std::ostream& operator<<(std::ostream& os, const DataAccessor& acc)
    {
        os << acc._acc;
        return os;
    }

    /**
     * @brief Return key count of json
     *
     * @param key
     * @return auto
     */
    auto count(const std::string& key)
    {
        return _acc.count(key);
    }

    /**
     * @brief read value per the accessor info
     *
     * @return std::string
     */
    std::string read(void)
    {
        if (_acc[typeKey] == "DBUS")
        {
            // return ReadDBusProperty(_acc["object"], _acc["interface"],
            // _acc["property"]);
            return "123"; // WIP
        }
        else
        {
            return "";
        }
    }

    /**
     * @brief write value via the accessor info
     *
     * @param val
     */
    void write([[maybe_unused]] const std::string& val)
    {
        return;
    }

  private:
    /**
     * @brief Check if a acc json has the "type" field.
     *
     * @param acc
     * @return true
     * @return false
     */
    bool isValid(const nlohmann::json& acc)
    {
        return (acc.count(typeKey) > 0);
    }

  private:
    /**
     * @brief hold json data for the accessor.
     *
     */
    nlohmann::json _acc;
};

} // namespace data_accessor
