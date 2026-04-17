#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

namespace lwipc {

class Executor {
 public:
  Executor();
  ~Executor();

  Executor(const Executor&) = delete;
  Executor& operator=(const Executor&) = delete;

  void post(std::function<void()> task);
  void stop();

 private:
  void run();

  std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<std::function<void()>> tasks_;
  bool stopping_{false};
  std::thread worker_;
};

}  // namespace lwipc
