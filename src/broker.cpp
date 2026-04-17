#include "lwipc/broker.hpp"

#include <algorithm>

namespace lwipc {

Broker::SubscriptionId Broker::subscribe(std::string topic, Callback cb) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto id = next_id_++;
  subscriptions_[topic].push_back(Subscription{id, std::move(cb)});
  return id;
}

void Broker::unsubscribe(const std::string& topic, SubscriptionId id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = subscriptions_.find(topic);
  if (it == subscriptions_.end()) {
    return;
  }

  auto& subs = it->second;
  subs.erase(std::remove_if(subs.begin(), subs.end(), [id](const Subscription& s) { return s.id == id; }),
             subs.end());

  if (subs.empty()) {
    subscriptions_.erase(it);
  }
}

void Broker::publish(const std::string& topic, MessageView message) const {
  std::vector<Callback> callbacks;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = subscriptions_.find(topic);
    if (it == subscriptions_.end()) {
      return;
    }

    callbacks.reserve(it->second.size());
    for (const auto& sub : it->second) {
      callbacks.push_back(sub.callback);
    }
  }

  for (const auto& cb : callbacks) {
    cb(message);
  }
}

}  // namespace lwipc
