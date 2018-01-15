
#include <cassert>
#include <list>
#include <map>
#include <set>

#include "bcpp_api/bc.hpp"

#include "Debug.h"
#include "DecisionMaker.h"
#include "Util.hpp"

using namespace bc;
using std::vector;
using std::make_pair;
using std::map;
using std::list;
using std::endl;
using std::set;

// TODO: cache more things from GameController
// Specifically, defined some things that only need to be updated once per turn (ie karbonite, and compute further updates locally), and some things that only need to be updated once per game.
// Actually, is it better to make calls across the swigged library interface to rust, or is it better to shadow local variables?

class Bot {
 public:
  explicit Bot(GameController &gc) : m_gc(gc), decision_maker(gc), m_team(gc.get_team()), m_planet(gc.get_planet()) {
    // nothing for now

  }

  void turn() {
    // TODO handle mars
    if (m_planet == Planet::Mars) {
      return;
    }

    UnitTally unit_tally;
    unit_tally.update(m_gc);

    const Goal goal(decision_maker.computeGoal(unit_tally));

    tryUnloadingAllFactories(unit_tally);

    // first address construction
    // try adding more factories
    if (goal.build_factories) {
      tryBlueprinting<UnitType::Factory>(unit_tally);
    }
    if (goal.build_rockets) {
      tryBlueprinting<UnitType::Rocket>(unit_tally);
    }
    // then try building stuff, since this is basically free.
    // TODO: coordinate worker actions better
    tryBuilding(unit_tally);


    if (goal.build_workers) {
      tryReplicatingOrProducingWorkers(unit_tally);
    }
    if (goal.build_knights) {
      tryProducing(UnitType::Knight, unit_tally);
    }
    if (goal.build_rangers) {
      tryProducing(UnitType::Ranger, unit_tally);
    }
    if (goal.build_mages) {
      tryProducing(UnitType::Mage, unit_tally);
    }
    if (goal.build_healers) {
      tryProducing(UnitType::Healer, unit_tally);
    }

    // These seem mutually exclusive. Maybe make an enum.
    if (goal.attack) {
      moveAllUnitsTowardEnemies(unit_tally);
    } else if (goal.contain) {
      moveAllUnitsTowardEnemies(unit_tally);
    } else if (goal.defend) {
      moveAllUnitsTowardEnemies(unit_tally);
    }

    //moveIdleWorkersToKarbonite();
    //collectKarbonite();
  }

  void tryUnloadingAllFactories(UnitTally &unit_tally) {
    // TODO: for each factory, allow units to submit a request of which direction they'd like to be unloaded in
    // That way factories can essentially function as open space for pathfinding.
    for (const unsigned int &factory_id : unit_tally.units_by_type[UnitType::Factory]) {
      const Unit &factory = unit_tally.ids_to_units.at(factory_id);
      size_t num_inside = factory.get_structure_garrison().size();
      for (int direction_index = 0; direction_index < 8 && num_inside > 0; ++direction_index) {
        if (m_gc.can_unload(factory_id, directions_shuffled[direction_index])) {
          m_gc.unload(factory_id, directions_shuffled[direction_index]);
          --num_inside;
        }
      }
    }
  }

  template<UnitType StructType>
  void tryBlueprinting(UnitTally &tally) {
    // try to build a factory with each worker
    if (m_gc.get_karbonite() < unit_type_get_blueprint_cost(StructType)) {
      return;
    }
    for (const unsigned int &worker_id : tally.units_by_type[UnitType::Worker]) {
      if (m_workers_tasked_to_build.find(worker_id) != m_workers_tasked_to_build.end()) {
        // already building something. adding new stuff won't finish any faster.
        continue;
      }
      const Unit &worker = tally.ids_to_units.at(worker_id);
      for (const auto &d : directions_shuffled) {
        if (m_gc.can_blueprint(worker_id, UnitType::Factory, d)) {
          m_gc.blueprint(worker_id, UnitType::Factory, d);
          MapLocation target_loc = worker.get_map_location().add(d);
          Unit &blueprint = tally.add(m_gc.sense_unit_at_location(target_loc));
          m_construction_sites_to_workers[blueprint.get_id()].push_back(worker_id);
          m_workers_tasked_to_build.insert(worker_id);
        }
      }
    }
  }

  void tryBuilding(UnitTally &tally) {
    // check if any buildings are under construction
    list<unsigned int> finished;
    if (!m_construction_sites_to_workers.empty()) {
      for (auto site_iter = m_construction_sites_to_workers.begin();
           site_iter != m_construction_sites_to_workers.end();) {
        auto &site = *site_iter;
        if (!m_gc.has_unit(site.first)) {
          // factory was destroyed. delete while iterating.
          m_construction_sites_to_workers.erase(site_iter++);
        } else {
          for (auto worker_iter = site.second.cbegin(); worker_iter != site.second.cend();) {
            const unsigned int &worker_id = *worker_iter;
            if (!m_gc.has_unit(worker_id)) {
              // worker was kill. delete while iterating.
              site.second.erase(worker_iter++);
            } else {
              MapLocation worker_loc = getMapLocationOrGarrisonMapLocation(m_gc.get_unit(worker_id), m_gc);
              if (!worker_loc.is_adjacent_to(m_gc.get_unit(site.first).get_map_location())) {
                // worker moved away. delete while iterating.
                site.second.erase(worker_iter++);
              } else {
                if (!m_gc.get_unit(worker_id).worker_has_acted()) {
                  m_gc.build(worker_id, site.first);
                  // need to check a fresh copy of this variable every time
                  if (m_gc.get_unit(site.first).structure_is_built()) {
                    finished.push_back(site.first);
                    break; // all done!
                  }
                }
                ++worker_iter;
              }
            }
          }
          ++site_iter;
        }
      }
    }

    for (
      const auto &factory_id : finished
        ) {
      for (const auto &worker_id : m_construction_sites_to_workers[factory_id]) {
        m_workers_tasked_to_build.erase(worker_id);
      }
      m_construction_sites_to_workers.erase(factory_id);
    }

    // TODO: have nearby workers path toward factories? and add them to the list once they're close enough?
  }

  void tryReplicatingOrProducingWorkers(UnitTally &unit_tally) {
    unsigned int karbonite = m_gc.get_karbonite();
    unsigned int replicate_cost = unit_type_get_replicate_cost();
    if (karbonite >= replicate_cost) {
      list<unsigned int> replicated_worker_ids;
      // iterate through workers and try to clone
      for (const unsigned int &worker_id : unit_tally.units_by_type[UnitType::Worker]) {
        const Unit &worker = unit_tally.ids_to_units.at(worker_id);
        if (!worker.get_location().is_on_map()) {
          // in a garrison (or in space? is that possible to sense?
          continue;
        }
        // TODO: is there a constant for this?
        if (worker.get_ability_heat() < 10) {
          MapLocation worker_loc = worker.get_map_location();
          for (const Direction &d : directions_shuffled) {
            MapLocation target = worker_loc.add(d);
            const PlanetMap &map = m_gc.get_starting_planet(m_planet);
            if (target.get_x() < 0
                || target.get_y() < 0
                || target.get_x() >= map.get_width()
                || target.get_y() >= map.get_height()) {
              continue;
            }
            if (m_gc.is_occupiable(target)) {
              m_gc.replicate(worker.get_id(), d);
              karbonite -= replicate_cost;
              replicated_worker_ids.push_back(m_gc.sense_unit_at_location(target).get_id());
              // TODO: if goals overlap with each other, consider updating the Unit data (ability heat) right now.
              break;
            }
          }
        }
        if (karbonite < replicate_cost) {
          break;
        }
      }
      for (const unsigned int worker_id : replicated_worker_ids) {
        unit_tally.add(m_gc.get_unit(worker_id));
      }
    }

    tryProducing(UnitType::Worker, unit_tally);
  }

  void tryProducing(const UnitType &type, UnitTally &unit_tally) {
    const auto &factory_entry = unit_tally.units_by_type.find(UnitType::Factory);
    if (factory_entry == unit_tally.units_by_type.end()) {
      return;
    }
    list<unsigned int> &factory_ids = factory_entry->second;

    unsigned int karbonite = m_gc.get_karbonite();
    unsigned int cost = unit_type_get_factory_cost(type);
    if (karbonite >= cost) {
      // iterate through factories and try to produce
      for (const unsigned int &factory_id : factory_ids) {
        const Unit &factory = unit_tally.ids_to_units.at(factory_id);
        // TODO: is there a constant for this?
        if (!factory.structure_is_built() || factory.is_factory_producing() ||
            factory.get_structure_garrison().size() == 8) {
          continue;
        }
        m_gc.produce_robot(factory_id, type);
        karbonite -= cost;
        // TODO: make a local wrapper around factory, so we can update the is_producing status without re-fetching
        // update the factory status, so further calls to is_factory_producing() return false
        unit_tally.ids_to_units.insert(make_pair(factory_id, m_gc.get_unit(factory_id)));
        if (karbonite < cost) {
          break;
        }
      }
    }
  }

  void moveAllUnitsTowardEnemies(const UnitTally &tally) {
    // TODO: store enemy unit locations in a more abstract way, ie with an EnemyUnitTracker, sort of like the allied UnitTally
    // TODO: make a separate class for this, because a lot of variable state needs to be transferred between enemy-detection code, micro code, and long-distance pathing code

    list<Unit> enemy_units;
    const auto all_units = m_gc.get_units();
    for (const Unit &unit : all_units) {
      if (unit.get_team() == m_team) {
        continue;
      }
      enemy_units.push_back(unit);
    }

    // maps can be at most 50x50
    // this means there could be 2500 units on a map at once
    // we can do about 5,000,000 operations in 50ms. 2500^2 is 6,250,000, which means we can only do it if we have time saved up from earlier.

    list<const Unit *> safe, needs_micro;
    checkDangerZone(enemy_units, tally, safe, needs_micro);
    // TODO: create a quad-tree or bucketing data structure (if we only care about queries of a few known radii), so we can efficiently look up which units are near another unit.
    tryMicroing(enemy_units, needs_micro);

    tryMoveTowardEnemies(enemy_units, safe);
  }

  void checkDangerZone(const list<Unit> enemy_units, const UnitTally &unit_tally, list<const Unit *> &safe,
                       list<const Unit *> &unsafe) {
    // the slow way
    for (const auto &type_with_list : unit_tally.units_by_type) {
      if (type_with_list.first == UnitType::Factory || type_with_list.first == UnitType::Rocket) {
        continue;
      }
      for (const unsigned int &our_unit_id : type_with_list.second) {
        const Unit &our_unit = unit_tally.ids_to_units.at(our_unit_id);
        bool is_safe = true;
        MapLocation our_loc = getMapLocationOrGarrisonMapLocation(our_unit, m_gc);
        unsigned int our_range_sq = our_unit.get_attack_range();
        for (const auto &enemy_unit : enemy_units) {
          if (our_unit.get_unit_type() == UnitType::Worker) {
            UnitType enemy_type = enemy_unit.get_unit_type();
            if (enemy_unit.is_structure() || enemy_type == UnitType::Worker || enemy_type == UnitType::Healer) {
              // don't be afeared
              continue;
            }
          }
          // worst case: we step toward the enemy, and the enemy steps toward us
          // (or it's a structure and something is about to pop out!)
          MapLocation enemy_loc = getMapLocationOrGarrisonMapLocation(enemy_unit, m_gc);
          MapLocation one_step = our_loc.add(our_loc.direction_to(enemy_loc));
          MapLocation two_steps = one_step.add(one_step.direction_to(enemy_loc));
          unsigned int distsq = two_steps.distance_squared_to(enemy_loc);
          // TODO: get the ranger max range as a constant
          int enemy_range = enemy_unit.is_structure() ? 50 : enemy_unit.get_attack_range();
          // TODO: take javelins and other abilities into account
          if (distsq <= our_range_sq || (distsq <= enemy_range)) {
            is_safe = false;
            break;
          }
        }

        if (is_safe) {
          // workers can do whatever they were doing before
          if (our_unit.get_unit_type() != UnitType::Worker) {
            safe.push_back(&our_unit);
          }
        } else {
          unsafe.push_back(&our_unit);
        }
      }
    }
  }

  void tryMicroing(list<Unit> &enemy_units, const list<const Unit *> units) {
    // just move toward the enemy and attack when in range
    // again, these lookups are super slow

    for (const Unit *our_unit : units) {
      MapLocation our_loc = getMapLocationOrGarrisonMapLocation(*our_unit, m_gc);
      const Unit *closest_enemy = nullptr;
      MapLocation closest_maploc;
      unsigned int closest_distsq = 2 * 51 * 51;
      for (auto enemy_iter = enemy_units.cbegin(); enemy_iter != enemy_units.cend();) {
        const Unit &enemy_unit = *enemy_iter;
        // it might already be dead
        if (!m_gc.has_unit(enemy_unit.get_id())) {
          // remove from list while iterating
          enemy_units.erase(enemy_iter++);
          continue;
        }
        MapLocation enemy_loc = getMapLocationOrGarrisonMapLocation(enemy_unit, m_gc);
        unsigned int distsq = our_loc.distance_squared_to(enemy_loc);
        if (distsq < closest_distsq) {
          closest_enemy = &enemy_unit;
          closest_maploc = enemy_loc;
          closest_distsq = distsq;
        }
        ++enemy_iter;
      }

      if (closest_enemy == nullptr) {
        // false alarm
        // TODO: add back to safe list
        continue;
      }

      if (our_unit->get_unit_type() == UnitType::Worker) {
        // run away!
        Direction away = closest_maploc.direction_to(our_loc);
        pathInDirection(*our_unit, away);
      } else {
        // are we out of range?
        bool in_range;
        if (closest_distsq <= our_unit->get_attack_range()) {
          LOG("already close!" << endl);
          in_range = true;
        } else {
          LOG("pathing to!" << endl);
          pathTo(*our_unit, closest_maploc);
          Location loc = m_gc.get_unit(our_unit->get_id()).get_location();
          if (loc.is_on_map()) {
            LOG("pathed!" << endl);
            MapLocation our_new_loc = loc.get_map_location();
            in_range = our_new_loc.distance_squared_to(closest_maploc) <= our_unit->get_attack_range();

            LOG("now in range? " << in_range << endl);
          } else {
            LOG("in garrison!" << endl);
            in_range = false;
          }
        }

        if (in_range) {
          // TODO: replace this constant
          LOG("our unit: " << m_gc.get_unit(our_unit->get_id()) << endl);
          LOG("their unit: " << m_gc.get_unit(closest_enemy->get_id()) << endl);
          if (our_unit->get_attack_heat() < 10) {
            m_gc.attack(our_unit->get_id(), closest_enemy->get_id());
          }
        }
      }
    }

  }

  void tryMoveTowardEnemies(const list<Unit> &enemy_units, const list<const Unit *> units) {
    // just pick one of the starting locations and go toward it
    // change it every few turns to mix things up
    const vector<Unit> &initial_units = m_gc.get_starting_planet(m_planet).get_initial_units();
    vector<const Unit *> initial_enemy_units;
    for (const auto &unit : initial_units) {
      if (unit.get_team() != m_team) {
        initial_enemy_units.push_back(&unit);
      }
    }

    unsigned int target_idx = (m_gc.get_round() / 100U) % static_cast<unsigned int>(initial_enemy_units.size());
    MapLocation target = initial_enemy_units[target_idx]->get_map_location();

    for (const Unit *our_unit : units) {
      pathTo(*our_unit, target);
    }
  }

  const vector<int> rotations_toward = {0, 1, -1};
  const vector<int> rotations_toward_or_sideways = {0, 1, -1, 2, -2};
  const vector<int> rotations_sort_of_toward = {0, 1, -1, 2, -2, 3, -3};

  void pathTo(const Unit &unit, const MapLocation &target) {
    if (!unit.is_on_map()) {
      // this unit is still garrisoned.
      return;
    }

    MapLocation unit_loc = unit.get_map_location();
    Direction dir_to_target = unit.get_map_location().direction_to(target);
    pathInDirection(unit, dir_to_target);
  }

  void pathInDirection(const Unit &unit, const Direction &target_dir) {
    // TODO: is there a constant for this?
    if (unit.get_movement_heat() >= 10) {
      return;
    }
    unsigned int id = unit.get_id();
    for (int rot : rotations_sort_of_toward) {
      auto dir = static_cast<Direction>((target_dir + rot) % 8);
      if (m_gc.can_move(id, dir)) {
        m_gc.move_robot(id, dir);
        break;
      }
    }
  }

 private:
  GameController &m_gc;
  DecisionMaker decision_maker;
  const Team m_team;
  const Planet m_planet;

  map<unsigned int, list<unsigned int>> m_construction_sites_to_workers;
  set<unsigned int> m_workers_tasked_to_build;

};


int main(int argv, char **argc) {

  srand(0);

  GameController gc;
  Bot bot(gc);

  CHECK_ERRORS();

  // loop through the whole game.
  while (true) {
    print_status_update(gc);
    shuffle_directions();
    bot.turn();

    CHECK_ERRORS();

    fflush(stdout);
    gc.next_turn();
  }
}