#pragma once

#include <sys/types.h>

#include <cstddef>

namespace kindle {
enum class PowerEvent { None, GoingToSleep, ReadyToSuspend, Waking, ListenerFailed };

// Own one read-only LIPC subscriber. The input loop polls its pipe, independently
// of the render worker. A fixed buffer avoids allocations while reading events.
class PowerMonitor {
 public:
  ~PowerMonitor() { stop(); }
  PowerMonitor() = default;
  PowerMonitor(const PowerMonitor&) = delete;
  PowerMonitor& operator=(const PowerMonitor&) = delete;
  bool start(const char* command = "/usr/bin/lipc-wait-event");
  PowerEvent poll();
  void stop();
  static PowerEvent parse(const char* line);

 private:
  int fd_ = -1;
  pid_t child_ = -1;
  char line_[160]{};
  size_t used_ = 0;
  bool overflow_ = false, failed_ = false;
};
}  // namespace kindle
