#pragma once
namespace kindle {
// Single-threaded stock CLI calls with a one-second deadline and owned cleanup.
bool logPowerState(const char* stage, const char* command = "/usr/bin/lipc-get-prop");
bool setStatusBarVisible(bool visible, const char* command = "/usr/bin/lipc-set-prop");
}  // namespace kindle
