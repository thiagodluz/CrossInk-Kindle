#include "KindleSystem.h"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace kindle {
namespace {
bool closeOnExec(int fd) { return fcntl(fd, F_SETFD, FD_CLOEXEC) == 0; }
uint64_t milliseconds() {
  timespec now{};
  clock_gettime(CLOCK_MONOTONIC, &now);
  return static_cast<uint64_t>(now.tv_sec) * 1000 + now.tv_nsec / 1000000;
}
struct Result {
  char value[128]{};
  int exitCode = -1;
  bool timedOut = false, overflow = false;
  bool ok() const { return exitCode == 0 && !timedOut && !overflow; }
};
Result run(const char* command, const char* const* arguments) {
  int output[2];
  if (pipe(output) != 0) return {};
  if (!closeOnExec(output[0]) || !closeOnExec(output[1]) || fcntl(output[0], F_SETFL, O_NONBLOCK) != 0) {
    close(output[0]);
    close(output[1]);
    return {};
  }
  const pid_t parent = getpid();
  const pid_t child = fork();
  if (child == 0) {
    close(output[0]);
    if (prctl(PR_SET_PDEATHSIG, SIGKILL) == 0 && getppid() == parent && dup2(output[1], STDOUT_FILENO) >= 0) {
      close(output[1]);
      execve(command, const_cast<char* const*>(arguments), environ);
    }
    _exit(127);
  }
  close(output[1]);
  if (child < 0) {
    close(output[0]);
    return {};
  }
  Result captured;
  char* value = captured.value;
  size_t used = 0;
  bool ended = false, overflow = false;
  const uint64_t deadline = milliseconds() + 1000;
  int status = 0;
  pid_t result = 0;
  while (milliseconds() < deadline) {
    pollfd descriptor{output[0], POLLIN, 0};
    if (!ended) ::poll(&descriptor, 1, 20);
    for (unsigned i = 0; i < 256 && !ended; ++i) {
      char byte;
      const auto count = read(output[0], &byte, 1);
      if (count == 0) ended = true;
      if (count <= 0) break;
      if (used + 1 < sizeof(captured.value))
        value[used++] = byte == '\n' || byte == '\r' ? ' ' : byte;
      else
        overflow = true;
    }
    if (result == 0) {
      do {
        result = waitpid(child, &status, WNOHANG);
      } while (result < 0 && errno == EINTR);
    }
    if (result != 0 && ended) break;
    if (ended) usleep(10000);
  }
  const bool timedOut = result == 0;
  if (timedOut) {
    kill(child, SIGKILL);
    do {
      result = waitpid(child, &status, 0);
    } while (result < 0 && errno == EINTR);
  }
  close(output[0]);
  while (used && value[used - 1] == ' ') value[--used] = '\0';
  captured.exitCode = result == child && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  captured.timedOut = timedOut;
  captured.overflow = overflow;
  return captured;
}
}  // namespace
bool logPowerState(const char* stage, const char* command) {
  const char* arguments[] = {command, "com.lab126.powerd", "state", nullptr};
  const auto result = run(command, arguments);
  std::printf("power_state stage=%s value=%s exit=%d timeout=%d truncated=%d\n", stage,
              *result.value ? result.value : "unavailable", result.exitCode, result.timedOut, result.overflow);
  return result.ok() && *result.value;
}
bool setStatusBarVisible(bool visible, const char* command) {
  const char* arguments[] = {command, "com.lab126.pillow", "interrogatePillow",
                             visible
                                 ? "{\"pillowId\": \"default_status_bar\", \"function\": \"nativeBridge.showMe();\"}"
                                 : "{\"pillowId\": \"default_status_bar\", \"function\": \"nativeBridge.hideMe();\"}",
                             nullptr};
  const auto result = run(command, arguments);
  std::printf("status_bar=%s exit=%d timeout=%d truncated=%d\n", visible ? "show" : "hide", result.exitCode,
              result.timedOut, result.overflow);
  return result.ok();
}
bool recoverDisplay(const char* query, const char* command) {
  const char* stateArguments[] = {query, "com.lab126.powerd", "state", nullptr};
  const auto state = run(query, stateArguments);
  if (!state.ok() || !*state.value) {
    std::puts("display_recovery=failed reason=unknown-power-state");
    return false;
  }
  if (!std::strcmp(state.value, "screenSaver") || !std::strcmp(state.value, "suspended")) {
    std::printf("display_recovery=skipped power_state=%s\n", state.value);
    return true;
  }
  if (std::strcmp(state.value, "active")) {
    std::printf("display_recovery=failed power_state=%s\n", state.value);
    return false;
  }
  const char* arguments[] = {command, "com.lab126.appmgrd", "start", "app://com.lab126.booklet.home", nullptr};
  const auto result = run(command, arguments);
  std::printf("display_recovery=home exit=%d timeout=%d truncated=%d\n", result.exitCode, result.timedOut,
              result.overflow);
  return result.ok();
}
}  // namespace kindle
