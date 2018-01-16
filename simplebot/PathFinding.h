
#ifndef BC18_SCAFFOLD_PATHFINDING_H
#define BC18_SCAFFOLD_PATHFINDING_H

#include <cstdint>
#include <memory>
#include <vector>

#include <bcpp_api/bc.hpp>

class PathFinder {
 public:
  // if maps ever become larger than around 128x128, this needs to be made larger
  using DistType = uint16_t;
  // no need to call api for MapLocation constructors here.
  using RowCol = std::pair<DistType, DistType>;

  explicit PathFinder(const bc::GameController &gc, const bc::PlanetMap &map)
      : m_gc(gc),
        m_map(map),
        m_sidelength(m_map.get_width()),
        m_infinity((m_sidelength + 2) *
                   (m_sidelength + 2)),
        m_planet(m_map.get_planet()) {
  }


  void computeAllPairsShortestPath();

  void computeConnectedComponents();

 private:

  DistType index(const RowCol &rc);

  DistType index(const DistType &row, const DistType &col);

  DistType index(const bc::MapLocation &loc);

  void bfs(const bc::MapLocation &start, std::vector<DistType> &distances);

  const bc::GameController &m_gc;
  const bc::PlanetMap &m_map;
  const DistType m_sidelength;
  const DistType m_infinity;
  const bc::Planet m_planet;
};

#endif //BC18_SCAFFOLD_PATHFINDING_H
