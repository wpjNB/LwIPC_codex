#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "lwipc/message.hpp"

namespace lwipc {

class Broker {
 public:
  using SubscriptionId = std::uint64_t;
  using Callback = std::function<void(const MessageView&)>;

  SubscriptionId subscribe(std::string topic, Callback cb);
  void unsubscribe(const std::string& topic, SubscriptionId id);
  void publish(const std::string& topic, MessageView message) const;

 private:
  struct Subscription {
    SubscriptionId id;
    Callback callback;
  };

  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::vector<Subscription>> subscriptions_;
  SubscriptionId next_id_{1};
};

class Publisher {
 public:
  Publisher(Broker& broker, std::string topic, QoSProfile qos)
      : broker_(broker), topic_(std::move(topic)), qos_(qos) {}

  void publish(MessageView message) const { broker_.publish(topic_, message); }
  [[nodiscard]] const QoSProfile& qos() const { return qos_; }

 private:
  Broker& broker_;
  std::string topic_;
  QoSProfile qos_;
};

class Subscriber {
 public:
  Subscriber(Broker& broker, std::string topic, QoSProfile qos, Broker::Callback cb)
      : broker_(broker), topic_(std::move(topic)), qos_(qos), id_(broker_.subscribe(topic_, std::move(cb))) {}

  ~Subscriber() { broker_.unsubscribe(topic_, id_); }

  Subscriber(const Subscriber&) = delete;
  Subscriber& operator=(const Subscriber&) = delete;

 private:
  Broker& broker_;
  std::string topic_;
  QoSProfile qos_;
  Broker::SubscriptionId id_;
};

}  // namespace lwipc
