
#ifndef BC18_SCAFFOLD_UTIL_H
#define BC18_SCAFFOLD_UTIL_H

#include <vector>
#include "bcpp_api/bc.hpp"

extern const std::vector<bc::Direction> directions_incl_center;

extern const std::vector<bc::Direction> directions_cwise;
extern std::vector<bc::Direction> directions_shuffled;

bc::MapLocation getMapLocationOrGarrisonMapLocation(const bc::Unit& unit, const bc::GameController& gc);
void shuffle_directions();

#endif //BC18_SCAFFOLD_UTIL_H
