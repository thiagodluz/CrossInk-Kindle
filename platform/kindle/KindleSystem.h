#pragma once
#include <cstdint>
namespace kindle {
// Single-threaded stock CLI calls with a one-second deadline and owned cleanup.
bool logPowerState(const char* stage, const char* command = "/usr/bin/lipc-get-prop");
bool setStatusBarVisible(bool visible, const char* command = "/usr/bin/lipc-set-prop");
bool recoverDisplay(const char* query = "/usr/bin/lipc-get-prop", const char* command = "/usr/bin/lipc-set-prop");
// Renew the stock inactivity timer only on real input, at most every 30 seconds.
class UserActivity {
 public:
  bool notify(uint64_t now, const char* command = "/usr/bin/lipc-set-prop");

 private:
  uint64_t last_ = 0;
  bool attempted_ = false;
};
}  // namespace kindle
