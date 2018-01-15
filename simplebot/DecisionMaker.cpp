

#include "DecisionMaker.h"

using namespace bc;

Goal DecisionMaker::computeGoal(const UnitTally &unit_tally) {
  Goal result = Goal().set_attack();

  if (unit_tally.getCount(UnitType::Worker) < 5) {
    result.set_build_workers();
  }

  if (unit_tally.getCount(UnitType::Factory) < 1
      || m_gc.get_karbonite() >= unit_type_get_blueprint_cost(UnitType::Factory)) {
    result.set_build_factories();
  }

  // try to mantain 1:2 knight-to-worker ratio
  if (unit_tally.getCount(UnitType::Factory) >= 1) {
    if (unit_tally.getCount(UnitType::Worker) * 2 > unit_tally.getCount(UnitType::Knight)) {
      result.set_build_knights();
    } else {
      result.set_build_workers();
    }
  }

  return result;
}