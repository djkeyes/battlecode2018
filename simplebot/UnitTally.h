

#ifndef BC18_SCAFFOLD_UNITTALLY_H
#define BC18_SCAFFOLD_UNITTALLY_H

#include <map>
#include <list>

#include "bcpp_api/bc.hpp"
#include "Debug.h"

/*
 * Some utility functions for prettifying unit counting. This is essentially just a map.
 * At the start of every turn, users should update the map contents to account for dead units.
 */
class UnitTally {
 public:
  // TODO: Wrap bc::Unit with a struct. That way if we want to make an incremental update to a Unit, we only have to
  // update the variables that changed (and we could mark them as mutable), rather than re-copying the entire Unit.
  std::map<bc::UnitType, std::list<unsigned int>> units_by_type;
  std::map<unsigned int, bc::Unit> ids_to_units;

  /*
   * Loads the current allied unit list. This should be called once after construction.
   */
  void update(bc::GameController &gc);

  /*
   * Add a unit that was create *this turn*. Notably, this unit may not be able to do much yet.
   */
  bc::Unit &add(const bc::Unit &unit);

  /*
   * Get the current amount of a unit. If you need more information about the units, probably better to access the map directly.
   */
  unsigned int getCount(bc::UnitType type) const {
    const auto &typelist = units_by_type.find(type);
    if (typelist == units_by_type.end()) {
      return 0;
    }
    return typelist->second.size();
  }

};


#endif //BC18_SCAFFOLD_UNITTALLY_H
