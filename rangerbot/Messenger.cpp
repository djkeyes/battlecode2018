

#include "Messenger.h"

#include <vector>

using namespace bc;

using std::vector;
using std::list;

void Messenger::sendLandingLocations(const list<MapLocation> &locations) {

  m_gc.write_team_array(0, locations.size());
  int i = 0;
  for (auto iter = locations.begin(); iter != locations.end(); ++iter, ++i) {
    m_gc.write_team_array(2 * i + 1, iter->get_x());
    m_gc.write_team_array(2 * i + 2, iter->get_y());
  }
}

void Messenger::readLandingLocations(list<MapLocation> &locations) {
  // TODO: should cache if we read it frequently
  vector<int> team_array = m_gc.get_team_array(Planet::Mars);

  unsigned int size = team_array[0];
  for (unsigned int i = 0; i < size; ++i) {
    unsigned int x = team_array[2 * i + 1];
    unsigned int y = team_array[2 * i + 2];
    locations.push_back(MapLocation(Planet::Mars, x, y));
  }
}