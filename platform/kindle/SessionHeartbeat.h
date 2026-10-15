#pragma once
#include <cstdint>

namespace kindle {
// A nonblocking anonymous pipe, inherited only by this launch's processes.
// Each thread owns its pulse object; neither disk writes nor a timer thread
// can make a stalled input/render loop appear healthy.
class SessionHeartbeat {
 public:
  bool begin(char channel);
  bool pulse();

 private:
  int fd_ = -1;
  char channel_ = 0;
  uint64_t last_ = 0;
};
uint64_t monotonicMilliseconds();
}  // namespace kindle
