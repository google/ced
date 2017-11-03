#pragma once

#include "yaml-cpp/yaml.h"
#include "absl/synchronization/mutex.h"

class ConfigWatcher {
 public:
  ~ConfigWatcher();

 protected:
  void RegisterWatcher(const std::string& path);

 private:
  friend class ConfigRegistry;
  virtual void Set(YAML::Node node) = 0;
};

template <class T>
class Config final : public ConfigWatcher {
 public:
  Config(const std::string& path, const T& def = T()) : default_(def), value_(def) {
    RegisterWatcher(path);
  }

  T get() const {
    absl::MutexLock lock(&mu_);
    return value_;
  }

 private:
  mutable absl::Mutex mu_;
  const T default_;
  T value_ GUARDED_BY(mu_);

  void Set(YAML::Node node) {
    absl::MutexLock lock(&mu_);
    value_ = node.as<T>(default_);
  }
};

template <>
class Config<std::string> final : public ConfigWatcher {
 public:
  Config(const std::string& path, const std::string& def = std::string()) : default_(def), value_(def) {
    RegisterWatcher(path);
  }

  std::string get() const {
    absl::MutexLock lock(&mu_);
    return value_;
  }

 private:
  mutable absl::Mutex mu_;
  const std::string default_;
  std::string value_ GUARDED_BY(mu_);

  void Set(YAML::Node node) {
    absl::MutexLock lock(&mu_);
    value_ = node.Scalar();
  }
};

