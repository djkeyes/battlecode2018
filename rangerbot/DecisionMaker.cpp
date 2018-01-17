

#include "DecisionMaker.h"

#include <algorithm>

using namespace bc;
using std::min;
using std::max;

Goal DecisionMaker::computeGoal(const UnitTally &unit_tally, const MapPreprocessor &map_preprocessor) {
  Goal result = Goal().set_attack();

  // just eyeballing this limit. each worker mines enough for 4 more workers, assuming enemy takes half the map.
  // TODO: also take into account dat sweet marz ca$h
  if (unit_tally.getCount(UnitType::Worker) < max(6U, min(40U, map_preprocessor.totalKarbonite() / (2 * 15 * 4)))) {
    result.set_build_workers();
  }

  if (unit_tally.getCount(UnitType::Factory) < 1
      || m_gc.get_karbonite() >= unit_type_get_blueprint_cost(UnitType::Factory)) {
    result.set_build_factories();
  }

  // try to mantain 1:8 ranger-to-mage ratio
  if (unit_tally.getCount(UnitType::Factory) >= 1) {
    // this conditional makes a mage the second spawn. I guess that's cool?
    if (unit_tally.getCount(UnitType::Ranger) > 8 * unit_tally.getCount(UnitType::Mage)) {
      result.set_build_mages();
    } else {
      result.set_build_rangers();
    }
  }

  return result;
}
