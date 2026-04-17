#pragma once

#include <functional>
#include <memory>
#include <string>

#include "lwipc/broker.hpp"
#include "lwipc/message.hpp"

namespace lwipc {

class Channel {
 public:
  Channel(Broker& broker, std::string topic, QoSProfile qos)
      : pub_(broker, topic, qos), topic_(std::move(topic)), qos_(qos) {}

  void publish(MessageView message) { pub_.publish(message); }
  [[nodiscard]] const std::string& topic() const { return topic_; }
  [[nodiscard]] const QoSProfile& qos() const { return qos_; }

 private:
  Publisher pub_;
  std::string topic_;
  QoSProfile qos_;
};

class Node {
 public:
  explicit Node(std::string name) : name_(std::move(name)) {}

  std::shared_ptr<Channel> create_channel(const std::string& topic, const QoSProfile& qos) {
    return std::make_shared<Channel>(broker_, topic, qos);
  }

  std::unique_ptr<Subscriber> subscribe(const std::string& topic, const QoSProfile& qos,
                                        Broker::Callback cb) {
    return std::make_unique<Subscriber>(broker_, topic, qos, std::move(cb));
  }

  [[nodiscard]] const std::string& name() const { return name_; }

 private:
  std::string name_;
  Broker broker_;
};

}  // namespace lwipc
