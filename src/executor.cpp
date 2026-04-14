#include "lwipc/executor.hpp"

namespace lwipc {

Executor::Executor() : worker_([this] { run(); }) {}

Executor::~Executor() { stop(); }

void Executor::post(std::function<void()> task) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_) {
      return;
    }
    tasks_.push(std::move(task));
  }
  cv_.notify_one();
}

void Executor::stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_) {
      return;
    }
    stopping_ = true;
  }
  cv_.notify_all();
  if (worker_.joinable()) {
    worker_.join();
  }
}

void Executor::run() {
  while (true) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });

      if (stopping_ && tasks_.empty()) {
        return;
      }

      task = std::move(tasks_.front());
      tasks_.pop();
    }

    task();
  }
}

}  // namespace lwipc
