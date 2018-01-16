
#include "UnitTally.h"

#include <cassert>

using namespace bc;
using std::make_pair;

void UnitTally::update(GameController &gc) {
  for (auto &unit : gc.get_my_units()) {
    unsigned int unit_id = unit.get_id();
    units_by_type[unit.get_unit_type()].push_back(unit_id);
    ids_to_units.insert(make_pair(unit_id, unit));
  }
}


Unit &UnitTally::add(const bc::Unit &unit) {
  // TODO: maybe add it to a separate list, if a user wants robots built only this turn?
  auto &units = units_by_type[unit.get_unit_type()];
  unsigned int unit_id = unit.get_id();
  units.push_back(unit_id);
  auto status = ids_to_units.insert(std::make_pair(unit_id, unit));
  // If this fails, the unit already existed in the list
  assert(status.second);
  return status.first->second;
}