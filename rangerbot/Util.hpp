
#ifndef BC18_SCAFFOLD_UTIL_H
#define BC18_SCAFFOLD_UTIL_H

#include <vector>
#include "bcpp_api/bc.hpp"

extern const std::vector<bc::Direction> directions_incl_center;

extern const std::vector<bc::Direction> directions_cwise;
extern std::vector<bc::Direction> directions_shuffled;

bc::MapLocation getMapLocationOrGarrisonMapLocation(const bc::Unit &unit, const bc::GameController &gc);

void shuffle_directions();

/*
 * Fast integer ceil(x/y). This only works for non-negative x and positive y.
 */
template<class IntType>
inline IntType pos_int_div_ceil(IntType x, IntType y) {
  // note: this can overflow in x + y
  return (x + y - static_cast<IntType>(1)) / y;
}

#endif //BC18_SCAFFOLD_UTIL_H
