#include "lwipc/topology.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace lwipc {

void StaticTopology::load_from_file(const std::string& path) {
  std::ifstream in(path);
  if (!in.is_open()) {
    throw std::runtime_error("failed to open topology file: " + path);
  }

  routes_.clear();
  topic_index_.clear();

  std::string line;
  std::size_t line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    if (line.empty() || line[0] == '#') {
      continue;
    }

    std::stringstream ss(line);
    std::string topic;
    std::string channel;
    std::string reliable_str;

    if (!std::getline(ss, topic, ',') || !std::getline(ss, channel, ',') || !std::getline(ss, reliable_str, ',')) {
      throw std::runtime_error("invalid topology line " + std::to_string(line_no));
    }

    const bool reliable = (reliable_str == "1" || reliable_str == "true" || reliable_str == "reliable");
    topic_index_[topic] = routes_.size();
    routes_.push_back(TopologyRoute{topic, channel, reliable});
  }
}

std::optional<TopologyRoute> StaticTopology::find(const std::string& topic) const {
  const auto it = topic_index_.find(topic);
  if (it == topic_index_.end()) {
    return std::nullopt;
  }

  return routes_[it->second];
}

}  // namespace lwipc
