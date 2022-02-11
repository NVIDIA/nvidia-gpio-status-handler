
/*
 *
 */

#pragma once

#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace dat_traverse {

enum class Status : int {
  succ,
  error,
  timeout,
};

enum ACCESSORTYPE { DBUS = 0, OTHER = 1 };

struct accessor {
  ACCESSORTYPE accessorType;
  std::vector<int> accessorValue;
};

class Device {
public:

  /* name of device */
  std::string name;

  /* downstream devices (children) */
  std::vector<std::string> association;

  /* upstream devices */
  std::vector<std::string> parents;

  /* 6-layer accessor info for this device */
  std::map<std::string, std::vector<accessor>> status;

  /* prints out in memory tree to verify population went as expected */
  static void printTree(const std::map<std::string, dat_traverse::Device>& m);

  /* populates memory structure with dat json contents */
  static void populateMap(std::map<std::string, dat_traverse::Device> &m,
                          const json& j);

  Device(const std::string& s);
  Device(const std::string& s, const json& j);
  ~Device();
};

class DATTraverse {
public:
  DATTraverse();
  ~DATTraverse();
};

} // namespace dat_traverse
