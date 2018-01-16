
#include "Debug.h"

using namespace bc;
using std::cout;
using std::endl;

void debug_print_status_update(const GameController &gc) {
#ifndef NDEBUG
  if (gc.get_team() == Team::Red) {
    LOG("RED-");
  } else {
    LOG("BLU-");
  }

  if (gc.get_planet() == Planet::Earth) {
    LOG("EART");
  } else {
    LOG("MARS");
  }

  LOG(" -- Turn " << gc.get_round());

  unsigned int time_left = gc.get_time_left_ms();
  static unsigned int last_time_left = 0;
  // 50ms free per turn
  unsigned int elapsed = last_time_left + 50 - time_left;
  last_time_left = time_left;
  LOG(" -- Time " << time_left << "ms");
  LOG(" -- Elpsd " << elapsed << "ms");

  LOG(" -- Karb " << gc.get_karbonite());


  const auto &units = gc.get_my_units();
  int num_factories = 0, num_rockets = 0, num_workers = 0, num_knights = 0, num_rangers = 0, num_mages = 0, num_healers = 0;
  for (const auto &unit : units) {
    switch (unit.get_unit_type()) {
      case UnitType::Factory:
        num_factories++;
        break;
      case UnitType::Rocket:
        num_rockets++;
        break;
      case UnitType::Worker:
        num_workers++;
        break;
      case UnitType::Knight:
        num_knights++;
        break;
      case UnitType::Ranger:
        num_rangers++;
        break;
      case UnitType::Mage:
        num_mages++;
        break;
      case UnitType::Healer:
        num_healers++;
        break;
    }
  }
  LOG(" -- Units [F:" << num_factories << "|R:" << num_rockets << "|W:" << num_workers << "|K:" << num_knights
                      << "|Ra:" << num_rangers << "|M:" << num_mages << "|H:" << num_healers << "]");

  LOG(endl);
#endif
}

std::ostream &operator<<(std::ostream &stream, const bc::Unit &unit) {
  stream << "{UNIT#" << unit.get_id() << " ";

  stream << "[team:";
  if (unit.get_team() == Blue) {
    stream << "BLU";
  } else {
    stream << "RED";
  }
  stream << "]";

  stream << "[type:";
  switch (unit.get_unit_type()) {
    case UnitType::Worker:
      stream << "Worker";
      break;
    case UnitType::Knight:
      stream << "Knight";
      break;
    case UnitType::Ranger:
      stream << "Ranger";
      break;
    case UnitType::Mage:
      stream << "Mage";
      break;
    case UnitType::Healer:
      stream << "Healer";
      break;
    case UnitType::Factory:
      stream << "Factory";
      break;
    case UnitType::Rocket:
      stream << "Rocket";
      break;
  }
  stream << "]";

  stream << "[health:" << unit.get_health() << "/" << unit.get_max_health() << "]";

  stream << "[loc:";
  Location loc = unit.get_location();
  if (loc.is_in_space()) {
    stream << "inspace";
    // TODO: get landing turn? or host rocket?
  } else if (loc.is_in_garrison()) {
    stream << "garrison#";
    stream << loc.get_structure();
  } else if (loc.is_on_map()) {
    stream << "maploc";
    MapLocation maploc = loc.get_map_location();
    // (row, col)
    stream << "(" << maploc.get_y() << "," << maploc.get_x() << ")";
  } else {
    stream << "???";
  }
  stream << "]";

  stream << "[heats:{";
  stream << "move:" << unit.get_movement_heat() << "|";
  stream << "atk:" << unit.get_attack_heat() << "|";
  stream << "abil:" << unit.get_ability_heat();
  stream << "}]";

  // TODO: worker has_acted
  // TODO: ranger is_sniping, countdown, target loc
  // TODO: structure is_built
  // TODO: structure garrison (list the IDs)
  // TODO: factory is_producing (and what, plus time left)

  return stream;
}


unsigned int debug_get_time_left(const GameController& gc) {
#ifdef NDEBUG
  return 0;
#else
  return gc.get_time_left_ms();
#endif
}