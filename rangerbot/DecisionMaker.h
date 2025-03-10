

#ifndef BC18_SCAFFOLD_DECISIONMAKER_H
#define BC18_SCAFFOLD_DECISIONMAKER_H

#include "bcpp_api/bc.hpp"
#include "UnitTally.h"
#include "MapPreprocessor.h"

struct Goal {

  Goal() : build_factories(false), build_workers(false), build_knights(false), build_rangers(false),
           build_mages(false), build_healers(false), build_rockets(false), attack(false), contain(false),
           defend(false), go_to_mars(false) {

  }

#define CHAINED_SETTER(name)\
    Goal& set_##name()\
    {\
        name = true;\
        return *this;\
    }\
    Goal& disable_##name()\
    {\
        name = false;\
        return *this;\
    }

  CHAINED_SETTER(build_factories);

  CHAINED_SETTER(build_workers);

  CHAINED_SETTER(build_knights);

  CHAINED_SETTER(build_rangers);

  CHAINED_SETTER(build_mages);

  CHAINED_SETTER(build_healers);

  CHAINED_SETTER(build_rockets);

  CHAINED_SETTER(attack);

  CHAINED_SETTER(contain);

  CHAINED_SETTER(defend);

  CHAINED_SETTER(go_to_mars);

  bool build_factories;
  bool build_workers;
  bool build_knights;
  bool build_rangers;
  bool build_mages;
  bool build_healers;
  bool build_rockets;
  bool attack;
  bool contain;
  bool defend;
  bool go_to_mars;
};

/*
 * Given the game state, return some abstract goals.
 * Ideally, a bot could choose from several possible decision-making strategies, or the mapping from game state to
 * goals could be a learned behavior.
 *
 * Decision maker implementation is tightly coupled to the kinds of goals that can be made and executes, unfortunately.
 * Furthermore, some goals are not mutually satisfiable, for example building knights AND building workers isn't
 * possible if you can only afford one. In this case, it might be biased toward one goal, ie building the cheaper unit
 * or building the unit that occurs first in the implementation code.
 * TODO: add a priority system
 * TODO: return goals as list of behaviors (function pointers) to execute, ordered by priority
 */
class DecisionMaker {
 public:
  DecisionMaker(bc::GameController &gc) : m_gc(gc) {}

  Goal computeGoal(const UnitTally &unit_tally, const MapPreprocessor &map_preprocessor);

 private:
  bc::GameController &m_gc;
};


#endif //BC18_SCAFFOLD_DECISIONMAKER_H
