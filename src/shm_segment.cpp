#include "lwipc/shm_segment.hpp"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdexcept>
#include <utility>

namespace lwipc {

namespace {
std::string normalize_name(std::string name) {
  if (name.empty()) {
    return "";
  }
  if (name.front() != '/') {
    name.insert(name.begin(), '/');
  }
  return name;
}
}  // namespace

ShmSegment::ShmSegment(std::string name, std::size_t size) : name_(normalize_name(std::move(name))), size_(size) {
  if (size_ == 0) {
    throw std::invalid_argument("shm size cannot be zero");
  }

  fd_ = shm_open(name_.c_str(), O_CREAT | O_RDWR, 0600);
  if (fd_ < 0) {
    throw std::runtime_error("shm_open failed");
  }

  if (ftruncate(fd_, static_cast<off_t>(size_)) != 0) {
    close();
    throw std::runtime_error("ftruncate failed");
  }

  addr_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
  if (addr_ == MAP_FAILED) {
    addr_ = nullptr;
    close();
    throw std::runtime_error("mmap failed");
  }
}

ShmSegment::~ShmSegment() { close(); }

ShmSegment::ShmSegment(ShmSegment&& other) noexcept
    : name_(std::move(other.name_)), size_(other.size_), fd_(other.fd_), addr_(other.addr_) {
  other.size_ = 0;
  other.fd_ = -1;
  other.addr_ = nullptr;
}

ShmSegment& ShmSegment::operator=(ShmSegment&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  close();
  name_ = std::move(other.name_);
  size_ = other.size_;
  fd_ = other.fd_;
  addr_ = other.addr_;

  other.size_ = 0;
  other.fd_ = -1;
  other.addr_ = nullptr;
  return *this;
}

bool ShmSegment::create(std::string name, std::size_t size) {
  name_ = normalize_name(std::move(name));
  size_ = size;
  
  if (name_.empty()) {
    return false;
  }
  
  if (size_ == 0) {
    return false;
  }

  fd_ = shm_open(name_.c_str(), O_CREAT | O_RDWR, 0600);
  if (fd_ < 0) {
    return false;
  }

  if (ftruncate(fd_, static_cast<off_t>(size_)) != 0) {
    ::close(fd_);
    fd_ = -1;
    return false;
  }

  addr_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
  if (addr_ == MAP_FAILED) {
    addr_ = nullptr;
    ::close(fd_);
    fd_ = -1;
    return false;
  }
  
  return true;
}

bool ShmSegment::open(std::string name) {
  name_ = normalize_name(std::move(name));
  
  if (name_.empty()) {
    return false;
  }

  fd_ = shm_open(name_.c_str(), O_RDWR, 0600);
  if (fd_ < 0) {
    return false;
  }

  // 获取共享内存大小
  struct stat sb;
  if (fstat(fd_, &sb) != 0) {
    ::close(fd_);
    fd_ = -1;
    return false;
  }
  size_ = static_cast<std::size_t>(sb.st_size);

  addr_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
  if (addr_ == MAP_FAILED) {
    addr_ = nullptr;
    ::close(fd_);
    fd_ = -1;
    return false;
  }
  
  return true;
}

void ShmSegment::close() {
  if (addr_ != nullptr) {
    munmap(addr_, size_);
    addr_ = nullptr;
  }

  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }

  if (!name_.empty()) {
    shm_unlink(name_.c_str());
  }
}

}  // namespace lwipc
