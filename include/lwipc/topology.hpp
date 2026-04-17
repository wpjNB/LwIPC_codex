#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace lwipc {

struct TopologyRoute {
  std::string topic;
  std::string channel;
  bool reliable{false};
};

class StaticTopology {
 public:
  void load_from_file(const std::string& path);
  [[nodiscard]] std::optional<TopologyRoute> find(const std::string& topic) const;
  [[nodiscard]] const std::vector<TopologyRoute>& routes() const { return routes_; }

 private:
  std::vector<TopologyRoute> routes_;
  std::unordered_map<std::string, std::size_t> topic_index_;
};

}  // namespace lwipc
