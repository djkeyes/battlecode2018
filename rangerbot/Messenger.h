

#ifndef RANGERBOT_MESSENGER_H
#define RANGERBOT_MESSENGER_H

#include <list>

#include <bcpp_api/bc.hpp>

class Messenger {
 public:
  explicit Messenger(bc::GameController &gc) : m_gc(gc) {}

  void sendLandingLocations(const std::list<bc::MapLocation> &locations);

  void readLandingLocations(std::list<bc::MapLocation> &locations);
  
  const int max_num_landing_locations = 30;

 private:
  bc::GameController &m_gc;
};


#endif //RANGERBOT_MESSENGER_H
