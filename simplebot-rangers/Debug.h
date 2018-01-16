

#ifndef BC18_SCAFFOLD_DEBUG_H
#define BC18_SCAFFOLD_DEBUG_H

#include <iostream>

#include "bcpp_api/bc.hpp"

#ifdef NDEBUG
#  define LOG(x)
#else
#  define LOG(x) do { (std::cout << x); } while(false)
#endif

void debug_print_status_update(const bc::GameController &gc);
unsigned int debug_get_time_left(const bc::GameController &gc);

/*
 * Pretty formatting for units
 */
std::ostream& operator<<(std::ostream& stream, const bc::Unit& unit);

#endif //BC18_SCAFFOLD_DEBUG_H
