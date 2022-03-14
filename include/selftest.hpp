
/*
 *
 */

#pragma once

#include "aml.hpp"
#include "object.hpp"

#include <nlohmann/json.hpp>

#include <ostream>
#include <string>

namespace selftest
{

aml::RcCode DoSelftest(const device::Device& dev, const std::string& report);

class Report
{
  public:
    Report() = default;
    ~Report() = default;

  public:
    nlohmann::json& GenerateReportHeader();

    friend ostream& operator<<(ostream& os, const Report& rpt);

  private:
    std::string _file;
    nlohmann::json _report;
};

ostream& operator<<(ostream& os, const Report& rpt)
{
    os << rpt._report;
    return os;
}

class Selftest : public object::Object
{
  public:
    Selftest(const std::string& name = __PRETTY_FUNCTION__) :
        object::Object(name)
    {}
    ~Selftest() = default;

  public:
    aml::RcCode perform([[maybe_unused]] const device::Device& dev)
    {
        return aml::RcCode::succ;
    }

  private:
    nlohmann::json _report;
};

} // namespace selftest
