#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace lwipc {

class HeartbeatMonitor {
 public:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;

  void register_peer(const std::string& peer_id, TimePoint now = Clock::now());
  void beat(const std::string& peer_id, TimePoint now = Clock::now());
  [[nodiscard]] std::vector<std::string> expired(TimePoint now,
                                                 std::chrono::milliseconds timeout) const;

 private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, TimePoint> last_seen_;
};

}  // namespace lwipc
