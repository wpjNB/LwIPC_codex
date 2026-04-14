#include "lwipc/topology.hpp"

#include <cassert>
#include <fstream>
#include <string>

int main() {
  const std::string path = "/tmp/lwipc_topology_test.csv";
  {
    std::ofstream out(path);
    out << "# topic,channel,reliable\n";
    out << "/camera/front,shm_cam,1\n";
    out << "/lidar/top,shm_lidar,0\n";
  }

  lwipc::StaticTopology topology;
  topology.load_from_file(path);

  assert(topology.routes().size() == 2);

  auto cam = topology.find("/camera/front");
  assert(cam.has_value());
  assert(cam->channel == "shm_cam");
  assert(cam->reliable);

  auto lidar = topology.find("/lidar/top");
  assert(lidar.has_value());
  assert(!lidar->reliable);

  auto missing = topology.find("/unknown");
  assert(!missing.has_value());

  return 0;
}
