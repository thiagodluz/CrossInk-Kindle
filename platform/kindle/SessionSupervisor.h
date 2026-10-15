#pragma once

namespace kindle {
struct SupervisorLimits {
  unsigned startupMs = 60000;
  unsigned inputMs = 15000;
  unsigned renderMs = 30000;
  unsigned cleanupMs = 10000;
};
// Single-threaded CLI entry, before display/input acquisition. Owns the runner
// child and a pipe; there is no session-duration limit while both loops pulse.
int superviseSession(const char* runner, const SupervisorLimits& limits = {});
}  // namespace kindle
