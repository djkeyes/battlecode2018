

#include "MapPreprocessor.h"

#include <algorithm>
#include <iostream>
#include <iomanip>

#include "Debug.h"

using namespace bc;
using std::make_pair;
using std::map;
using std::unordered_map;
using std::min;
using std::vector;
using std::cout;
using std::endl;
using std::unique_ptr;

void MapPreprocessor::process() {
  // compute passable tiles
  computePassableAndInitialKarbonite(m_passable, m_karbonite_on_map);

  // summarize initial karbonite
  summarizeInitialKarbonite(m_summarized_karbonite, m_coarse_tiles_with_karbonite_to_fine_tiles);

  if (m_planet == Planet::Mars) {
    cacheAsteroidStrikes(m_asteroid_strikes);
  }

  /*LOG("total karbonite: " << m_total_karbonite << endl);
  LOG("full karbonite map:" << endl);
  print_karbonite_map();
  LOG("summarized karbonite map:" << endl);
  print_karbonite_summary_map();*/

  m_path_finder.computeAllPairsShortestPath(m_passable);
  m_path_finder.computeConnectedComponents();
}

void MapPreprocessor::cacheAsteroidStrikes(
    unique_ptr<unordered_map<unsigned int, AsteroidStrike>> &asteroid_strikes) {
  asteroid_strikes.reset(new unordered_map<unsigned int, AsteroidStrike>(
      m_gc.get_asteroid_pattern().get_all_strikes()));
}

void MapPreprocessor::updateMarsKarboniteEachTurn() {
  if (m_asteroid_strikes) {
    unsigned int round = m_gc.get_round();
    auto iter = m_asteroid_strikes->find(round);
    if (iter != m_asteroid_strikes->end()) {
      const AsteroidStrike &strike = iter->second;
      MapLocation loc = strike.get_map_location();
      unsigned int added_karbs = strike.get_karbonite();

      RowCol rowcol(loc.get_y(), loc.get_x());
      unsigned int &karbs = m_karbonite_on_map[m_path_finder.index(rowcol)];
      karbs += added_karbs;

      m_total_karbonite += added_karbs;

      unsigned int coarse_index = fineLocationToCoarseIndex(rowcol);
      unsigned int &coarse_amount = m_summarized_karbonite[coarse_index];
      // new area!
      if (coarse_amount == 0) {
        m_coarse_tiles_with_karbonite_to_fine_tiles[coarse_index] = rowcol;
      }
      coarse_amount += added_karbs;
    }
  }
}

void MapPreprocessor::print_karbonite_map() {
#ifndef NDEBUG
  using std::setfill;
  using std::setw;
  for (DistType r_start = 0; r_start < m_rows; ++r_start) {
    for (DistType c_start = 0; c_start < m_cols; ++c_start) {
      LOG("|");
      unsigned int karbs = m_karbonite_on_map[m_path_finder.index(r_start, c_start)];
      LOG(setfill(' ') << setw(2) << karbs);
    }
    LOG("|" << endl);
  }
#endif
}

void MapPreprocessor::print_karbonite_summary_map() {
#ifndef NDEBUG
  using std::setfill;
  using std::setw;
  for (DistType r_start = 0; r_start < m_coarse_rows; ++r_start) {
    for (DistType c_start = 0; c_start < m_coarse_cols; ++c_start) {
      DistType index = coarseIndex(r_start, c_start);
      unsigned int karbs = m_summarized_karbonite[index];
      LOG("|");
      LOG(setfill(' ') << setw(2) << karbs);
    }
    LOG("|" << endl);
  }
#endif
}

void MapPreprocessor::computePassableAndInitialKarbonite(vector<bool> &passable, vector<unsigned int> &karbonite) {
  passable = vector<bool>(m_rows * m_cols);
  if (m_planet == Planet::Earth) {
    karbonite = vector<unsigned int>(m_rows * m_cols);
    for (DistType r = 0; r < m_rows; ++r) {
      for (DistType c = 0; c < m_cols; ++c) {
        MapLocation loc(m_planet, c, r);
        m_passable[m_path_finder.index(r, c)] = m_map.is_passable_terrain_at(loc);
        m_karbonite_on_map[m_path_finder.index(r, c)] = m_map.get_initial_karbonite_at(loc);
      }
    }
  } else {
    // no initial karbonite on mars
    karbonite = vector<unsigned int>(m_rows * m_cols, 0);
    for (DistType r = 0; r < m_rows; ++r) {
      for (DistType c = 0; c < m_cols; ++c) {
        MapLocation loc(m_planet, c, r);
        m_passable[m_path_finder.index(r, c)] = m_map.is_passable_terrain_at(loc);
      }
    }
  }
}

void MapPreprocessor::summarizeInitialKarbonite(vector<unsigned int> &coarse_karbonite,
                                                map<DistType, RowCol> &coarse_with_fine_tiles) {
  coarse_karbonite = vector<unsigned int>(m_coarse_rows * m_coarse_cols, 0);
  m_total_karbonite = 0;

  for (DistType coarse_row = 0; coarse_row < m_coarse_rows; ++coarse_row) {
    for (DistType coarse_col = 0; coarse_col < m_coarse_cols; ++coarse_col) {
      unsigned int &sum = coarse_karbonite[coarse_row * m_coarse_cols + coarse_col];
      unsigned int max_karbs = 0;
      DistType max_r, max_c;
      for (DistType r = coarse_row * karbonite_summary_grid_size;
           r < (coarse_row + 1) * karbonite_summary_grid_size; ++r) {
        for (DistType c = coarse_col * karbonite_summary_grid_size;
             c < (coarse_col + 1) * karbonite_summary_grid_size; ++c) {
          unsigned int karbs = m_karbonite_on_map[m_path_finder.index(r, c)];
          sum += karbs;
          // TODO: prefer locations near the center
          // this is probably good enough though
          if (karbs > max_karbs) {
            max_karbs = karbs;
            max_r = r;
            max_c = c;
          }
        }
      }
      m_total_karbonite += sum;

      if (max_karbs > 0) {
        // If anyone asks "where's a valid cell in that coarse area?", we can give them this.
        // Theoretically, it could be unreachable (disconnected map). But that's a risk we'll have to take.
        coarse_with_fine_tiles.insert(make_pair(coarseIndex(coarse_row, coarse_col), RowCol(max_r, max_c)));
      }

    }
  }
}

void MapPreprocessor::updateKarbonite(const MapLocation &loc, unsigned int observed_amount, bool may_be_unchanged) {
  RowCol rowcol(loc.get_y(), loc.get_x());
  unsigned int &karbs = m_karbonite_on_map[m_path_finder.index(rowcol)];
  unsigned int reduction = karbs - observed_amount;
  if (may_be_unchanged && reduction == 0) {
    return;
  }
  karbs = observed_amount;

  m_total_karbonite -= reduction;

  unsigned int coarse_index = fineLocationToCoarseIndex(rowcol);
  unsigned int &coarse_amount = m_summarized_karbonite[coarse_index];
  coarse_amount -= reduction;
  // this area is mined out!
  if (coarse_amount == 0) {
    m_coarse_tiles_with_karbonite_to_fine_tiles.erase(coarse_index);
  }
}

unsigned int MapPreprocessor::queryKarboniteIfNonzero(const MapLocation &loc) {
  RowCol rowcol(loc.get_y(), loc.get_x());
  if (m_karbonite_on_map[m_path_finder.index(rowcol)] == 0) {
    return 0;
  } else {
    unsigned int newly_observed_karbs = m_gc.get_karbonite_at(loc);
    updateKarbonite(loc, newly_observed_karbs, true);
    return newly_observed_karbs;
  }
}