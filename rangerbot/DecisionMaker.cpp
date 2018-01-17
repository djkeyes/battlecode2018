

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

    if (unit_tally.getCount(UnitType::Ranger) + unit_tally.getCount(UnitType::Mage) >= 40) {
      // TODO: if we call get_units_in_space more than once, we should cache it
      // don't have more than one rocket flying at onces, until it's getting later
      if (m_gc.get_round() > 600 || m_gc.get_units_in_space().empty()) {
        // feelin pretty safe
        if (m_gc.get_research_info().get_level(UnitType::Rocket) > 0
            && unit_tally.getCount(UnitType::Rocket) < 1) {
          result.set_build_rockets();
          result.disable_build_knights();
          result.disable_build_workers();
          result.disable_build_rangers();
          result.disable_build_mages();
          result.disable_build_healers();
        }
        result.set_go_to_mars();
      }
    }
  }


  return result;
}
