#include "lwipc/heartbeat.hpp"

namespace lwipc {

void HeartbeatMonitor::register_peer(const std::string& peer_id, TimePoint now) {
  std::lock_guard<std::mutex> lock(mutex_);
  last_seen_[peer_id] = now;
}

void HeartbeatMonitor::beat(const std::string& peer_id, TimePoint now) {
  std::lock_guard<std::mutex> lock(mutex_);
  last_seen_[peer_id] = now;
}

std::vector<std::string> HeartbeatMonitor::expired(TimePoint now, std::chrono::milliseconds timeout) const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::string> stale;
  stale.reserve(last_seen_.size());

  for (const auto& [peer_id, ts] : last_seen_) {
    if (now - ts > timeout) {
      stale.push_back(peer_id);
    }
  }
  return stale;
}

}  // namespace lwipc
