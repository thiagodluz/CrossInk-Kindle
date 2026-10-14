#pragma once
#include <sched.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "Print.h"
#define PROGMEM
#define IRAM_ATTR
#define DRAM_ATTR
#define RTC_NOINIT_ATTR
#define PGM_P const char*
#define PSTR(s) (s)
inline unsigned long millis() {
  timespec t{};
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000UL + t.tv_nsec / 1000000;
}
inline unsigned long micros() {
  timespec t{};
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000000UL + t.tv_nsec / 1000;
}
inline void delay(unsigned long ms) {
  while (ms) {
    const auto part = ms > 1000 ? 1000 : ms;
    usleep(part * 1000);
    ms -= part;
  }
}
inline void yield() { sched_yield(); }
struct KindleMemoryHints {
  uint32_t getFreeHeap() const;
  uint32_t getMaxAllocHeap() const { return getFreeHeap(); }
  void restart() const;
};
extern KindleMemoryHints ESP;
