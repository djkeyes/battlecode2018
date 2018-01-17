
#include <algorithm>
#include <cassert>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <cmath>

#include "bcpp_api/bc.hpp"

#include "Debug.h"
#include "DecisionMaker.h"
#include "MapPreprocessor.h"
#include "PathFinding.h"
#include "Util.hpp"
#include "Messenger.h"

using namespace bc;
using std::vector;
using std::make_pair;
using std::map;
using std::list;
using std::endl;
using std::set;
using std::unique_ptr;

// TODO: cache more things from GameController
// Specifically, defined some things that only need to be updated once per turn (ie karbonite, and compute further updates locally), and some things that only need to be updated once per game.
// Actually, is it better to make calls across the swigged library interface to rust, or is it better to shadow local variables?

class Bot {
 public:
  explicit Bot(GameController &gc) :
      m_gc(gc),
      decision_maker(gc),
      m_team(gc.get_team()),
      m_planet(gc.get_planet()),
      m_map(m_gc.get_starting_planet(m_planet)),
      m_path_finder(gc, m_map),
      m_map_preprocessor(gc, m_path_finder, m_map),
      m_messenger(gc) {
    // nothing for now

  }

  void beforeLoop() {
    m_map_preprocessor.process();

    if (m_planet == Planet::Earth) {
      // TODO: manage research smarter
      m_gc.queue_research(UnitType::Worker);
      m_gc.queue_research(UnitType::Ranger);
      m_gc.queue_research(UnitType::Mage);
      m_gc.queue_research(UnitType::Rocket);
      m_gc.queue_research(UnitType::Mage);
      m_gc.queue_research(UnitType::Mage);
      m_gc.queue_research(UnitType::Rocket);
      m_gc.queue_research(UnitType::Rocket);
      m_gc.queue_research(UnitType::Mage);
      m_gc.queue_research(UnitType::Worker);
      m_gc.queue_research(UnitType::Worker);
      m_gc.queue_research(UnitType::Worker);
    } else {
      // TODO
      // for now, just choose some random tiles that are multiple of 3 (to avoid collisions)
      list<MapLocation> landing_positions;
      set<unsigned int> hashes;
      for (int tries = 0;
           tries < m_messenger.max_num_landing_locations * 3
           && landing_positions.size() < m_messenger.max_num_landing_locations;
           ++tries) {
        unsigned int x = 3 * (rand() % (m_map.get_width() / 3));
        unsigned int y = 3 * (rand() % (m_map.get_height() / 3));
        if (!m_map_preprocessor.passable()[m_path_finder.index(y, x)]) {
          continue;
        }
        unsigned int hash = x * m_map.get_width() + y;
        if (hashes.insert(hash).second) {
          landing_positions.push_back(MapLocation(Planet::Mars, x, y));
        }
      }
      m_messenger.sendLandingLocations(landing_positions);
    }
  }

  void turn() {
    // TODO handle mars
    if (m_planet == Planet::Earth) {
      earthTurn();
    } else {
      marsTurn();
    }
  }

  list<MapLocation> m_landing_locations;

  void earthTurn() {

    if (m_gc.get_round() == 55) {
      // read team array
      m_messenger.readLandingLocations(m_landing_locations);
    }

    UnitTally unit_tally;
    unit_tally.update(m_gc);

    const Goal goal(decision_maker.computeGoal(unit_tally, m_map_preprocessor));

    tryUnloadingAll<Factory>(unit_tally);

    // first address construction
    // try adding more factories
    if (goal.build_factories) {
      tryBlueprinting<UnitType::Factory>(unit_tally);
    }
    if (goal.build_rockets) {
      tryBlueprinting<UnitType::Rocket>(unit_tally);
    }
    // then try building stuff, since this is basically free.
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

    if (goal.go_to_mars) {
      loadAndLaunchRockets(unit_tally);
    }

    // These seem mutually exclusive. Maybe make an enum.
    if (goal.attack) {
      moveAllUnitsTowardEnemies(unit_tally);
    } else if (goal.contain) {
      moveAllUnitsTowardEnemies(unit_tally);
    } else if (goal.defend) {
      moveAllUnitsTowardEnemies(unit_tally);
    }

    collectKarbonite(unit_tally);
  }

  void marsTurn() {
    UnitTally unit_tally;
    unit_tally.update(m_gc);

    m_map_preprocessor.updateMarsKarboniteEachTurn();

    // do we have any goals on mars (maybe in the future the attack/defence goals)? Just attack everything, right?

    tryUnloadingAll<Rocket>(unit_tally);

    moveAllUnitsTowardEnemies(unit_tally);

    collectKarbonite(unit_tally);
  }

  template<UnitType StructType>
  void tryUnloadingAll(UnitTally &unit_tally) {
    // TODO: for each building, allow units to submit a request of which direction they'd like to be unloaded in
    // That way structures can essentially function as open space for pathfinding.
    for (const unsigned int &building_id : unit_tally.units_by_type[StructType]) {
      const Unit &building = unit_tally.ids_to_units.at(building_id);
      size_t num_inside = building.get_structure_garrison().size();
      for (int direction_index = 0; direction_index < 8 && num_inside > 0; ++direction_index) {
        if (m_gc.can_unload(building_id, directions_shuffled[direction_index])) {
          m_gc.unload(building_id, directions_shuffled[direction_index]);
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
        if (m_gc.can_blueprint(worker_id, StructType, d)) {
          m_gc.blueprint(worker_id, StructType, d);
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

    for (const auto &factory_id : finished) {
      for (const auto &worker_id : m_construction_sites_to_workers[factory_id]) {
        m_workers_tasked_to_build.erase(worker_id);
      }
      m_construction_sites_to_workers.erase(factory_id);
    }

    // path nearby workers toward factories, and add them to the list once they're close enough

    for (const auto &worker_id : tally.units_by_type[UnitType::Worker]) {
      if (m_workers_tasked_to_build.find(worker_id) != m_workers_tasked_to_build.end()) {
        continue;
      }
      // Suppose a construction site has H health remaining and W workers tasked to it. It will finish in
      // F_1 = CEIL(H/(W*B)) turns (assuming workers add B health per turn).
      // If our new worker takes T > F_1 turns to arrive, obviously don't bother.
      // Otherwise, after T turns, the structure will have H - T*W*B health left, and will require a total of
      // F_2 = T + CEIL((H - T*W*B) / (W+1)) turns.
      // Don't bother going if F_1 - F_2 is small.

      const Unit worker = m_gc.get_unit(worker_id);
      if (worker.get_location().is_in_garrison()) {
        // too confusing to estimate
        continue;
      }
      const MapLocation worker_loc = worker.get_map_location();
      int best_improvement = 2;
      unsigned int best_site_id;
      bool has_better_site = false;

      int b = worker.get_worker_build_health();
      for (const auto &site : m_construction_sites_to_workers) {
        auto w = static_cast<int>(site.second.size());

        const Unit building = m_gc.get_unit(site.first);
        int h = building.get_max_health() - building.get_health();
        int f_1 = 0;
        if (w == 0) {
          // maybe someone else will get there
          ++w;
          f_1 = 30;
        }
        int f_1_denom = w * b;
        f_1 += pos_int_div_ceil(h, f_1_denom);

        // estimate travel time -- this isn't exact
        MapLocation building_loc = building.get_map_location();
        int t = static_cast<int>(round(
            1.1f * (m_path_finder.getDist(worker_loc, building_loc) * (worker.get_movement_cooldown() / 10.0f)
                    + worker.get_movement_heat() / 10.0f)));
        if (t >= f_1 || t > 30) {
          continue;
        }

        int f_2_denom = (w + 1) * b;
        int f_2 = t + pos_int_div_ceil(h - w * b * t, f_2_denom);;
        int improvement = f_1 - f_2;
        if (improvement > best_improvement) {
          best_improvement = improvement;
          best_site_id = building.get_id();
          has_better_site = true;
        }
      }

      if (!has_better_site) {
        continue;
      }
      // try pathing toward site
      MapLocation site_loc = m_gc.get_unit(best_site_id).get_map_location();
      PathFinder::DistType dist = m_path_finder.getDist(worker_loc, site_loc);
      // note: this is dist, not distsq
      if (dist > 1) {
        pathTo(worker, site_loc);
        // update worker after move
        Unit newworker = m_gc.get_unit(worker_id);
        tally.ids_to_units[worker_id] = newworker;
        dist = m_path_finder.getDist(newworker.get_map_location(), site_loc);
      }
      if (dist <= 1) {
        // add to data structure
        m_workers_tasked_to_build.insert(worker_id);
        m_construction_sites_to_workers[best_site_id].push_back(worker_id);
        // try building
        // TODO this requires lots of duplicated logic
      }

    }

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
            if (target.get_x() < 0
                || target.get_y() < 0
                || target.get_x() >= m_map.get_width()
                || target.get_y() >= m_map.get_height()) {
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

    // It's way cheaper and faster to replicate, so don't use a factory unless we have to
    if (unit_tally.units_by_type[UnitType::Worker].empty()) {
      tryProducing(UnitType::Worker, unit_tally);
    }
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
        unit_tally.ids_to_units[factory_id] = m_gc.get_unit(factory_id);
        if (karbonite < cost) {
          break;
        }
      }
    }
  }

  void loadAndLaunchRockets(UnitTally &unit_tally) {
    // absorb as many things as possible
    // maybe always try to have 1 worker and 1 attacher tho

    if (m_landing_locations.empty()) {
      return;
    }

    auto &rocket_list = unit_tally.units_by_type[UnitType::Rocket];
    for (auto rocket_iter = rocket_list.cbegin(); rocket_iter != rocket_list.cend();) {
      const unsigned int &rocket_id = *rocket_iter;
      const Unit &rocket = m_gc.get_unit(rocket_id);
      // TODO compute the max garrison correctly
      unsigned int space_left = 8U - static_cast<unsigned int>(rocket.get_structure_garrison().size());
      if (space_left != 0) {
        // could do this more efficiently
        // also could do this in outward-spiral order. oh well.
        MapLocation rocket_loc = rocket.get_map_location();
        auto nearby_allies = m_gc.sense_nearby_units_by_team(rocket_loc, 8, m_team);
        for (const Unit &nearby : nearby_allies) {
          if (nearby.is_structure()) {
            continue;
          }
          unsigned int nearby_id = nearby.get_id();
          if (m_gc.can_load(rocket_id, nearby_id)) {
            m_gc.load(rocket_id, nearby_id);
            --space_left;
          } else {
            if (pathNaivelyTo(nearby, rocket_loc)) {
              // update the unit
              unit_tally.ids_to_units[nearby_id] = m_gc.get_unit(nearby_id);
            }
          }

          if (space_left == 0) {
            break;
          }
        }
      }

      if (space_left == 0) {
        // TODO: we really ought to re-use or compute more dynamically
        MapLocation &dest = m_landing_locations.front();
        m_gc.launch_rocket(rocket_id, dest);
        m_landing_locations.pop_front();

        // remove from the game
        // goooolly this is expensive. we should use a set or something instead.
        const vector<unsigned int> unit_ids = m_gc.get_unit(rocket_id).get_structure_garrison();

        for (const unsigned int &unit_id :unit_ids) {
          UnitType type = unit_tally.ids_to_units[unit_id].get_unit_type();
          unit_tally.ids_to_units.erase(unit_id);
          unit_tally.units_by_type[type].remove(unit_id);
        }
        unit_tally.ids_to_units.erase(rocket_id);
        // erase rocket_id from units_by_type[Rocket] while iterating
        rocket_list.erase(rocket_iter++);
      } else {
        ++rocket_iter;
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
      if (our_unit->get_unit_type() == UnitType::Worker) {
        tryMicroingWorker(*our_unit, enemy_units);
      } else {
        // TODO: should split this logic up for different attackers
        tryMicroing(*our_unit, enemy_units);
      }
    }

  }

  // TODO: move to micro file
  const unsigned int effective_ranger_range = 72;
  const unsigned int effective_mage_range = 45;
  const unsigned int effective_knight_range = 5;

  unsigned int getEffectiveRange(const UnitType &type) {
    // range + 1 movement
    switch (type) {
      case UnitType::Ranger:
        return effective_ranger_range;
      case UnitType::Mage:
        return effective_mage_range;
      case UnitType::Knight:
        return effective_knight_range;
    }
    return 0;
  }

  void tryMicroingWorker(const Unit &worker, list<Unit> &enemy_units) {
    MapLocation our_loc = getMapLocationOrGarrisonMapLocation(worker, m_gc);
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
      if (distsq < closest_distsq && distsq <= getEffectiveRange(enemy_unit.get_unit_type())) {
        closest_enemy = &enemy_unit;
        closest_maploc = enemy_loc;
        closest_distsq = distsq;
      }
      ++enemy_iter;
    }

    if (closest_enemy == nullptr) {
      // false alarm
      // TODO: add back to safe list
      return;
    }

    Direction away = closest_maploc.direction_to(our_loc);
    pathInDirection(worker, away);
  }

  void tryMicroing(const Unit &unit, list<Unit> &enemy_units) {
    MapLocation our_loc = getMapLocationOrGarrisonMapLocation(unit, m_gc);
    const Unit *closest_enemy = nullptr;
    MapLocation closest_maploc;
    unsigned int closest_distsq = 2 * 51 * 51;

    const Unit *weakest_attacker_in_range = nullptr;
    unsigned int weakest_attacker_health = 10000;
    const Unit *weakest_nonattacker_in_range = nullptr;
    unsigned int weakest_nonattacker_health = 10000;

    unsigned int my_range_sq = unit.get_attack_range();

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
      if (distsq <= my_range_sq) {
        unsigned int enemy_health = enemy_unit.get_health();
        if (enemy_unit.is_robot() && enemy_unit.get_damage() > 0) {
          if (enemy_health < weakest_attacker_health) {
            weakest_attacker_health = enemy_health;
            weakest_attacker_in_range = &enemy_unit;
          }
        } else {
          if (enemy_health < weakest_nonattacker_health) {
            weakest_nonattacker_health = enemy_health;
            weakest_nonattacker_in_range = &enemy_unit;
          }
        }
      }

      if (distsq < closest_distsq) {
        closest_enemy = &enemy_unit;
        closest_maploc = enemy_loc;
        closest_distsq = distsq;
      }
      ++enemy_iter;
    }

    // TODO: take into account situation where we take a step forward and they take a step back.
    // we can ignore anyone if it's still safe after that
    if (closest_enemy == nullptr) {
      // false alarm
      // TODO: add back to safe list
      return;
    }

    const Unit *best_target = nullptr;
    // are we out of range?
    bool in_range;
    if (weakest_attacker_in_range != nullptr) {
      in_range = true;
      best_target = weakest_attacker_in_range;
    } else if (weakest_nonattacker_in_range != nullptr) {
      in_range = true;
      best_target = weakest_nonattacker_in_range;
    } else {
      best_target = closest_enemy;
      if (closest_distsq <= my_range_sq) {
        in_range = true;
      } else {
        pathNaivelyTo(unit, closest_maploc);
        Location loc = m_gc.get_unit(unit.get_id()).get_location();
        if (loc.is_on_map()) {
          MapLocation our_new_loc = loc.get_map_location();
          in_range = our_new_loc.distance_squared_to(closest_maploc) <= my_range_sq;
        } else {
          in_range = false;
        }
      }
    }
    // TODO: if we haven't moved yet, we might still want to move somewhere
    // in large combat situations, we should probably just move away from the closest enemy
    // in 1:1 situations, we should kite slower units with shorter range, but charge units with the same range so we
    // get the first shot.

    if (in_range) {
      // TODO: replace this constant
      if (unit.get_attack_heat() < 10) {
        // TODO: this check shouldn't be necessary. why is in_range innaccurate?
        if (m_gc.can_attack(unit.get_id(), best_target->get_id())) {
          m_gc.attack(unit.get_id(), best_target->get_id());
        }
      }
    }
  }

  void tryMoveTowardEnemies(const list<Unit> &enemy_units, const list<const Unit *> units) {
    if (m_planet == Planet::Earth) {
      // TODO: detect split map. On split map, spreading out (or at least moving in a circle) gives more mobility.
      tryMoveToStartingLocations(units);
    } else {
      tryMovingInACircle(units);
    }
  }

  unique_ptr<list<MapLocation>> m_circle_path;
  list<MapLocation>::iterator m_circle_iter;

  void tryMovingInACircle(const list<const Unit *> units) {
    // draw a rectangle 1/3 away from each border
    // if we're lucky, every edge point will be on the map
    if (!m_circle_path) {
      m_circle_path.reset(new list<MapLocation>());

      auto h = static_cast<PathFinder::DistType>(m_map.get_height());
      auto w = static_cast<PathFinder::DistType>(m_map.get_width());

      PathFinder::DistType left = w / 3;
      PathFinder::DistType right = 2 * w / 3;
      PathFinder::DistType top = h / 3;
      PathFinder::DistType bot = 2 * h / 3;
      for (unsigned int i = left; i < right; ++i) {
        if (m_map_preprocessor.passable()[m_path_finder.index(top, i)]) {
          m_circle_path->push_back(MapLocation(m_planet, i, top));
        }
      }
      for (unsigned int i = top; i < bot; ++i) {
        if (m_map_preprocessor.passable()[m_path_finder.index(i, right)]) {
          m_circle_path->push_back(MapLocation(m_planet, right, i));
        }
      }
      for (unsigned int i = right; i > left; --i) {
        if (m_map_preprocessor.passable()[m_path_finder.index(bot, i)]) {
          m_circle_path->push_back(MapLocation(m_planet, i, bot));
        }
      }
      for (unsigned int i = bot; i > top; --i) {
        if (m_map_preprocessor.passable()[m_path_finder.index(i, left)]) {
          m_circle_path->push_back(MapLocation(m_planet, left, i));
        }
      }
      m_circle_iter = m_circle_path->begin();
    }

    // we were very unlucky :(
    if (m_circle_path->empty()) {
      return;
    }

    if (m_circle_iter == m_circle_path->end()) {
      m_circle_iter = m_circle_path->begin();
    }

    MapLocation target = *m_circle_iter;

    for (const Unit *our_unit : units) {
      pathTo(*our_unit, target);
    }

    if (m_gc.get_round() % 4 == 0) {
      ++m_circle_iter;
    }
  }

  void tryMoveToStartingLocations(const list<const Unit *> units) {
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

  /*
   * For long-distance pathing, take advantage of the pre-computed shortest path
   */
  void pathTo(const Unit &unit, const MapLocation &target) {
    if (unit.get_movement_heat() >= 10) {
      return;
    }
    if (!unit.is_on_map()) {
      // this unit is still garrisoned.
      return;
    }
    MapLocation unit_loc = unit.get_map_location();
    unsigned int id = unit.get_id();

    const Direction *best_dir = nullptr;
    // we could set this to the center direction if we wanted to do no worse.
    PathFinder::DistType best_dist = m_path_finder.infinity();
    for (const auto &dir: directions_shuffled) {
      MapLocation next = unit_loc.add(dir);

      if (!m_gc.can_move(id, dir)) {
        continue;
      }
      PathFinder::DistType dist = m_path_finder.getDist(next, target);
      if (dist < best_dist) {
        best_dist = dist;
        best_dir = &dir;
      }
    }

    if (best_dir != nullptr) {
      m_gc.move_robot(id, *best_dir);
    }

  }

  bool pathNaivelyTo(const Unit &unit, const MapLocation &target) {
    if (!unit.is_on_map()) {
      // this unit is still garrisoned.
      return false;
    }

    MapLocation unit_loc = unit.get_map_location();
    Direction dir_to_target = unit.get_map_location().direction_to(target);
    return pathInDirection(unit, dir_to_target);
  }

  bool pathInDirection(const Unit &unit, const Direction &target_dir) {
    // TODO: is there a constant for this?
    if (unit.get_movement_heat() >= 10) {
      return false;
    }
    unsigned int id = unit.get_id();
    for (int rot : rotations_sort_of_toward) {
      auto dir = static_cast<Direction>((target_dir + rot) % 8);
      if (m_gc.can_move(id, dir)) {
        m_gc.move_robot(id, dir);
        return true;
      }
    }
  }

  void collectKarbonite(UnitTally &unit_tally) {
    if (m_map_preprocessor.coarseKarboniteLocationsToAnyFineLocations().empty()) {
      // map exhausted
      return;
    }
    for (const auto &worker_id : unit_tally.units_by_type[UnitType::Worker]) {
      if (!m_gc.has_unit(worker_id)) {
        // might've died to friendly fire. FIXME
        continue;
      }
      const Unit worker = m_gc.get_unit(worker_id);
      if (worker.worker_has_acted()) {
        continue;
      }
      if (worker.get_location().is_in_garrison()) {
        continue;
      }

      MapLocation worker_loc = worker.get_map_location();
      if (tryHarvestingKarbs(worker.get_worker_harvest_amount(), worker_loc, worker_id)) {
        continue;
      }

      // no karbonite nearby? explore!
      if (worker.get_movement_heat() >= 10) {
        continue;
      }
      bool moved = false;
      // check the coarse karbonite buckets, and path toward the nearest one
      const auto &coarse_locs = m_map_preprocessor.coarseKarboniteLocationsToAnyFineLocations();
      if (coarse_locs.empty()) {
        // map exhausted
        break;
      }
      PathFinder::RowCol loc(worker_loc.get_y(), worker_loc.get_x());
      // are we already in a place with karbonite?
      if (coarse_locs.find(m_map_preprocessor.fineLocationToCoarseIndex(loc)) != coarse_locs.end()) {
        // if so, try exploring.
        for (int num_steps = 2; num_steps <= 4 && !moved; ++num_steps) {
          for (const Direction &dir : directions_shuffled) {
            MapLocation target = worker_loc.add_multiple(dir, num_steps);
            if (m_path_finder.is_in_map_bounds(target) && m_map_preprocessor.queryKarboniteIfNonzero(target) > 0) {
              pathTo(worker, target);
              moved = true;
              break;
            }
          }
        }
      } else {
        // no karbonite near me! find the closest coarse location with karbonite!
        PathFinder::DistType closest_dist = m_path_finder.infinity();
        PathFinder::RowCol closest;
        for (const auto &coarse_and_fine : coarse_locs) {
          PathFinder::DistType dist = m_path_finder.getDist(loc, coarse_and_fine.second);
          if (dist < closest_dist) {
            closest_dist = dist;
            closest = coarse_and_fine.second;
          }
        }
        if (closest_dist < m_path_finder.infinity()) {
          pathTo(worker, MapLocation(m_planet, closest.second, closest.first));
          moved = true;
        }
      }
      if (!moved) {
        // just move anywhere possible
        for (const Direction &dir : directions_shuffled) {
          if (m_gc.can_move(worker_id, dir)) {
            pathTo(worker, worker_loc.add(dir));
            break;
          }
        }
      }

      // try again (TODO: only check the newly adjacent tiles)
      const Unit updated_worker = m_gc.get_unit(worker_id);
      tryHarvestingKarbs(updated_worker.get_worker_harvest_amount(), updated_worker.get_map_location(), worker_id);
    }
  }

  bool tryHarvestingKarbs(const unsigned int worker_harvest_amount,
                          const MapLocation &worker_loc,
                          const unsigned int &worker_id) {
    unsigned int most_karbs = 0;
    const Direction *best_dir;
    for (const Direction &dir : directions_incl_center) {
      MapLocation loc = worker_loc.add(dir);
      if (!m_path_finder.is_in_map_bounds(loc)) {
        continue;
      }
      unsigned int karbs = m_map_preprocessor.queryKarboniteIfNonzero(loc);
      if (karbs > most_karbs) {
        most_karbs = karbs;
        best_dir = &dir;
      }
    }
    if (most_karbs > 0) {
      m_gc.harvest(worker_id, *best_dir);
      m_map_preprocessor.updateKarbonite(worker_loc, most_karbs - std::min(most_karbs, worker_harvest_amount), false);
      return true;
    }
    return false;
  }

 private:
  GameController &m_gc;
  DecisionMaker decision_maker;
  const Team m_team;
  const Planet m_planet;
  const PlanetMap &m_map;
  PathFinder m_path_finder;
  MapPreprocessor m_map_preprocessor;
  Messenger m_messenger;

  map<unsigned int, list<unsigned int>> m_construction_sites_to_workers;
  set<unsigned int> m_workers_tasked_to_build;

};


int main(int argv, char **argc) {

  srand(0);

  GameController gc;
  Bot bot(gc);

  CHECK_ERRORS();

  bot.beforeLoop();

  // loop through the whole game.
  while (true) {
    debug_print_status_update(gc);
    shuffle_directions();
    bot.turn();

    CHECK_ERRORS();

    fflush(stdout);
    gc.next_turn();
  }
}
