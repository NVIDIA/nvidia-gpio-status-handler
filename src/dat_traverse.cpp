
/*
 *
 */

#include "dat_traverse.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <ostream>
#include <queue>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace dat_traverse {

/* test */
DATTraverse::DATTraverse() {}

DATTraverse::~DATTraverse() {}

Device::~Device() {}

void Device::populateMap(std::map<std::string, dat_traverse::Device> &m,
                         const json& j) {
  // initial map population
  // will map deviceName to device itself and fill out all children
  // (association)
  for (const auto &el : j.items()) {
    auto deviceName = el.key();
    dat_traverse::Device device(deviceName, el.value());
    m.insert(std::pair<std::string, dat_traverse::Device>(deviceName, device));
  }

  // fill out parents of all child devices on 2nd pass
  for (const auto &entry : m) {
    for (const auto &child : entry.second.association) {
      m.at(child).parents.push_back(entry.first);
    }
  }
}

void Device::printTree(const std::map<std::string, dat_traverse::Device>& m) {
  std::queue<std::string> fringe;
  fringe.push("Baseboard");
  while (!fringe.empty()) {
    std::string deviceName = fringe.front();
    fringe.pop();

    dat_traverse::Device device = m.at(deviceName);

    std::cerr << "Found Device " << deviceName << "\n";
    for (const auto &parent : device.parents) {
      std::cerr << parent << " is a parent of " << deviceName << "\n";
    }

    for (const auto &child : device.association) {
      std::cerr << "Found child " << child << " of device " << deviceName
                << "\n";
      fringe.push(child);
    }
    std::cerr << "\n";
  }
}

Device::Device(const std::string& name) { this->name = name; }

Device::Device(const std::string& name, const json& j) {

  this->name = name;
  this->association = j["association"].get<std::vector<std::string>>();

  /* TODO: populate eventInfo map with 6 layer info for each device once
   * information collected in JSON */
  // this->eventInfo.insert(std::pair<std::string,
  // std::vector<accessor>>("power_rail", )    )
}

} // namespace dat_traverse
