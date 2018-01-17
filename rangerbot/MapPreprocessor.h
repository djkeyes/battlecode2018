

#ifndef RANGERBOT_MAPPREPROCESSOR_H
#define RANGERBOT_MAPPREPROCESSOR_H

#include <map>
#include "bcpp_api/bc.hpp"

#include "PathFinding.h"
#include "Util.hpp"

class MapPreprocessor {
 public:
  MapPreprocessor(const bc::GameController &gc, PathFinder &path_finder, const bc::PlanetMap &map)
      : m_gc(gc),
        m_path_finder(path_finder),
        m_map(map),
        m_rows(static_cast<DistType >(m_map.get_height())),
        m_cols(static_cast<DistType >(m_map.get_width())),
        m_coarse_rows(pos_int_div_ceil(m_rows, karbonite_summary_grid_size)),
        m_coarse_cols(pos_int_div_ceil(m_cols, karbonite_summary_grid_size)),
        m_planet(m_map.get_planet()) {
  }

  using DistType = PathFinder::DistType;
  using RowCol = PathFinder::RowCol;

  const DistType karbonite_summary_grid_size = 5;

  // TODO: make the different kinds of preprocessing optional. unfortunately some depend on others, so it's tricy.
  void process();

  std::vector<bool> &passable() { return m_passable; }

  std::vector<unsigned int> &karboniteLocations() { return m_karbonite_on_map; }

  std::map<DistType, RowCol> &coarseKarboniteLocationsToAnyFineLocations() {
    return m_coarse_tiles_with_karbonite_to_fine_tiles;
  }

  /*
   * To be called when a user mines karbonite at the map location with coords mined_location.
   */
  void updateKarbonite(const RowCol &mined_location, unsigned int &mine_amount);

  const unsigned int &totalKarbonite() { return m_total_karbonite; }

 private:
  void computePassableAndInitialKarbonite(std::vector<bool> &passable, std::vector<unsigned int> &karbonite);

  void summarizeInitialKarbonite(std::vector<unsigned int> &coarse_karbonite,
                                 std::map<DistType, RowCol> &coarse_with_fine_tiles);

  void print_karbonite_map();

  void print_karbonite_summary_map();

  std::vector<bool> m_passable;
  std::vector<unsigned int> m_karbonite_on_map;
  std::vector<unsigned int> m_summarized_karbonite;
  std::map<DistType, RowCol> m_coarse_tiles_with_karbonite_to_fine_tiles;
  unsigned int m_total_karbonite;

  DistType coarseIndex(DistType coarse_row, DistType coarse_col) {
    return coarse_row * m_coarse_cols + coarse_col;
  }

  const bc::GameController &m_gc;
  PathFinder &m_path_finder;
  const bc::PlanetMap &m_map;
  const DistType m_rows;
  const DistType m_cols;
  const DistType m_coarse_rows;
  const DistType m_coarse_cols;
  const bc::Planet m_planet;
};


#endif //RANGERBOT_MAPPREPROCESSOR_H
