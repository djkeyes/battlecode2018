

#ifndef BC18_SCAFFOLD_DEBUG_H
#define BC18_SCAFFOLD_DEBUG_H

#include <iostream>

#include "bcpp_api/bc.hpp"

#ifdef NDEBUG
#  define LOG(x)
#else
#  define LOG(x) do { (std::cout << x); } while(false)
#endif

void print_status_update(bc::GameController &gc);

/*
 * Pretty formatting for units
 */
std::ostream& operator<<(std::ostream& stream, const bc::Unit& unit);

#endif //BC18_SCAFFOLD_DEBUG_H
