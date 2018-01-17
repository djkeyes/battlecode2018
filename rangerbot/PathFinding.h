
#ifndef BC18_SCAFFOLD_PATHFINDING_H
#define BC18_SCAFFOLD_PATHFINDING_H

#include <cstdint>
#include <memory>
#include <vector>

#include <bcpp_api/bc.hpp>

// TODO: on earth, also store some info about mars, so we can launch rockets.
class PathFinder {
 public:
  // if maps ever become larger than around 128x128, this needs to be made larger
  using DistType = uint16_t;
  // no need to call api for MapLocation constructors here.
  using RowCol = std::pair<DistType, DistType>;

  explicit PathFinder(const bc::GameController &gc, const bc::PlanetMap &map)
      : m_gc(gc),
        m_map(map),
        m_rows(static_cast<DistType >(m_map.get_height())),
        m_cols(static_cast<DistType >(m_map.get_width())),
        m_infinity((m_rows + static_cast<DistType >(2)) *
                   (m_cols + static_cast<DistType >(2))),
        m_planet(m_map.get_planet()) {
  }


  void computeAllPairsShortestPath(const std::vector<bool> &passable);

  void computeConnectedComponents();

  /*
   * Check if the location is in the map bounds using only the map dimensions (doesn't check correct planet).
   */
  bool is_in_map_bounds(const bc::MapLocation &loc) {
    int x = loc.get_x();
    int y = loc.get_y();
    return x >= 0 && y >= 0 && x < m_cols && y < m_rows;
  }

  DistType getDist(const RowCol &from, const RowCol &to) {
    return m_all_pair_distances[index(from)][index(to)];
  }

  DistType getDist(const DistType &from_row, const DistType &from_col, const DistType &to_row, const DistType &to_col) {
    return m_all_pair_distances[index(from_row, from_col)][index(to_row, to_col)];
  }

  DistType getDist(const bc::MapLocation &from, const bc::MapLocation &to) {
    return m_all_pair_distances[index(from)][index(to)];
  }

  DistType infinity() {
    return m_infinity;
  }

  DistType index(const RowCol &rc);

  DistType index(const DistType &row, const DistType &col);

  DistType index(const bc::MapLocation &loc);

 private:

  void bfs(const RowCol &start, std::vector<DistType> &distances, const std::vector<bool> &passable);

  std::vector<std::vector<DistType >> m_all_pair_distances;

  void print_dist_slice();

  const bc::GameController &m_gc;
  const bc::PlanetMap &m_map;
  const DistType m_rows;
  const DistType m_cols;
  const DistType m_infinity;
  const bc::Planet m_planet;
};

#endif //BC18_SCAFFOLD_PATHFINDING_H
