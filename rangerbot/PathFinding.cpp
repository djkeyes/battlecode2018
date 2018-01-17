

#include "PathFinding.h"

#include <list>
#include <iomanip>

#include "Debug.h"

using namespace bc;
using std::vector;
using std::list;
using std::endl;
using std::setfill;
using std::setw;


PathFinder::RowCol &operator+=(PathFinder::RowCol &lhs, const PathFinder::RowCol &rhs) {
  lhs.first += rhs.first;
  lhs.second += rhs.second;
  return lhs;
}

const PathFinder::RowCol operator+(const PathFinder::RowCol &lhs, const PathFinder::RowCol &rhs) {
  PathFinder::RowCol result = lhs;
  result += rhs;
  return result;
}

// Note that the are unsigned, so -1 gets cast to (2^N-1)
const vector<PathFinder::RowCol> dirs{
    {1,  0},
    {1,  1},
    {0,  1},
    {-1, 1},
    {-1, 0},
    {-1, -1},
    {0,  -1},
    {1,  -1}
};

PathFinder::DistType PathFinder::index(const RowCol &rc) {
  return index(rc.first, rc.second);
}

PathFinder::DistType PathFinder::index(const DistType &row, const DistType &col) {
  return row * m_cols + col;
}

PathFinder::DistType PathFinder::index(const MapLocation &loc) {
  return static_cast<DistType>(loc.get_y()) * m_cols + static_cast<DistType>(loc.get_x());
}

void PathFinder::computeAllPairsShortestPath(const vector<bool> &passable) {
  LOG("Starting all pairs shortest path computation!" << endl);
  unsigned int start_time_left = debug_get_time_left(m_gc);

  m_all_pair_distances = vector<vector<DistType >>(m_rows * m_cols,
                                                   vector<DistType>(m_rows * m_cols, m_infinity));

  for (DistType r_start = 0; r_start < m_rows; ++r_start) {
    for (DistType c_start = 0; c_start < m_cols; ++c_start) {
      bfs(RowCol(r_start, c_start), m_all_pair_distances[index(r_start, c_start)], passable);
    }
  }

  unsigned int end_time_left = debug_get_time_left(m_gc);
  LOG("Time remaining at start: " << start_time_left << "ms" << endl);
  LOG("Time remaining at end: " << end_time_left << "ms" << endl);
  LOG("Time elapsed: " << (start_time_left - end_time_left) << "ms" << endl);

}

void PathFinder::print_dist_slice() {
#ifndef NDEBUG
  for (DistType r_start = 0; r_start < m_rows; ++r_start) {
    for (DistType c_start = 0; c_start < m_cols; ++c_start) {
      LOG("|");
      DistType dist = m_all_pair_distances[index(r_start, c_start)][index(0, 5)];
      if (dist == m_infinity) {
        LOG(" - ");
      } else {
        LOG(setfill(' ') << setw(3) << dist);
      }
    }
    LOG("|" << endl);
  }
#endif
}

void PathFinder::bfs(const RowCol &start, vector<DistType> &distances, const vector<bool> &passable) {
  if (!passable[index(start)]) {
    return;
  }
  DistType next_dist_to_assign = 0;
  distances[index(start)] = next_dist_to_assign++;

  // TODO: might be faster to use stacks here. we could even pre-allocate vectors, since we can compute an upper
  // bound on the number of frontier vertices relatively easy
  list<RowCol> frontier, next_frontier;
  frontier.push_back(start);

  while (!frontier.empty()) {
    const RowCol &cur = frontier.front();

    for (const RowCol &dir : dirs) {
      const RowCol next = cur + dir;

      // bounds check
      // note that an index of -1 underflows to 2^N-1
      if (next.first >= m_rows || next.second >= m_cols) {
        continue;
      }
      DistType &next_dist = distances[index(next)];
      if (next_dist != m_infinity) {
        // already visited
        continue;
      }
      if (passable[index(next)]) {
        next_dist = next_dist_to_assign;
        next_frontier.push_back(next);
      }
    }

    frontier.pop_front();
    if (frontier.empty()) {
      std::swap(frontier, next_frontier);
      ++next_dist_to_assign;
    }
  }

}

void PathFinder::computeConnectedComponents() {}

