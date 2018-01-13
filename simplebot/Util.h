
#ifndef BC18_SCAFFOLD_UTIL_H
#define BC18_SCAFFOLD_UTIL_H

#include <vector>
#include "bcpp_api/bc.hpp"

const std::vector<bc::Direction> directions = {
        bc::Direction::North,
        bc::Direction::Northeast,
        bc::Direction::East,
        bc::Direction::Southeast,
        bc::Direction::South,
        bc::Direction::Southwest,
        bc::Direction::West,
        bc::Direction::Northwest
};

#endif //BC18_SCAFFOLD_UTIL_H
