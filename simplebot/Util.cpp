
#include <algorithm>

#include "Util.hpp"

using namespace bc;

const std::vector<Direction> directions_cwise = {
    Direction::North,
    Direction::Northeast,
    Direction::East,
    Direction::Southeast,
    Direction::South,
    Direction::Southwest,
    Direction::West,
    Direction::Northwest
};
std::vector<Direction> directions_shuffled = directions_cwise;

MapLocation getMapLocationOrGarrisonMapLocation(const Unit &unit, const GameController &gc) {
  // Not sure what to do if this unit is in space or on another planet.
  // ...is it even possible to sense a unit that's in space or on another planet?

  Location loc = unit.get_location();
  if (loc.is_in_garrison()) {
    return gc.get_unit(loc.get_structure()).get_map_location();
  } else {
    return loc.get_map_location();
  }
}

void shuffle_directions() {
  std::random_shuffle(directions_shuffled.begin(), directions_shuffled.end());
}

