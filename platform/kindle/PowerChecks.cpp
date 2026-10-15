#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "KindlePower.h"
#include "KindleSystem.h"

namespace {
unsigned failures = 0;
void check(bool passed, const char* what) {
  if (!passed) {
    ++failures;
    std::fprintf(stderr, "FAIL %s\n", what);
  }
}
}  // namespace

int main(int argc, char** argv) {
  using kindle::PowerEvent;
  using kindle::PowerMonitor;
  if (argc != 3) return 2;
  if (!std::strncmp(argv[2], "recovery-", 9)) {
    const std::string base = argv[1];
    const bool expected = !std::strcmp(argv[2], "recovery-active") || !std::strcmp(argv[2], "recovery-sleep");
    check(kindle::recoverDisplay((base + "/recovery-query").c_str(), (base + "/recovery-home").c_str()) == expected,
          "state-aware bounded display recovery");
    int status;
    check(waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD, "recovery helpers reaped");
    return failures ? 1 : 0;
  }
  if (!std::strncmp(argv[2], "bar-", 4)) {
    const bool visible = std::strstr(argv[2], "show") != nullptr;
    const bool expected = std::strstr(argv[2], "ok") != nullptr;
    check(kindle::setStatusBarVisible(visible, argv[1]) == expected, "bounded status bar result");
    int status;
    check(waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD, "status bar helper reaped");
    return failures ? 1 : 0;
  }
  if (!std::strncmp(argv[2], "state-", 6)) {
    const bool expected = !std::strcmp(argv[2], "state-ok");
    check(kindle::logPowerState("check", argv[1]) == expected, "bounded state query result");
    int status;
    check(waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD, "state helper reaped");
    return failures ? 1 : 0;
  }
  check(PowerMonitor::parse("goingToScreenSaver 2") == PowerEvent::GoingToSleep, "power button event");
  check(PowerMonitor::parse("goingToScreenSaver 4\r") == PowerEvent::GoingToSleep, "cover event");
  check(PowerMonitor::parse("goingToScreenSaver 0") == PowerEvent::GoingToSleep, "idle event");
  check(PowerMonitor::parse("readyToSuspend 0") == PowerEvent::ReadyToSuspend, "late suspend fallback");
  check(PowerMonitor::parse("wakeupFromSuspend 0") == PowerEvent::Waking, "wake fallback");
  check(PowerMonitor::parse("outOfScreenSaver 1") == PowerEvent::Waking, "screensaver exit");
  check(PowerMonitor::parse("goingToScreenSaverInvalid 2") == PowerEvent::None, "exact event names");
  check(PowerMonitor::parse("battLevelChanged 70") == PowerEvent::None, "unrelated power event");
  check(PowerMonitor::parse(nullptr) == PowerEvent::None, "null input");
  PowerMonitor monitor;
  check(!monitor.start("/crossink-missing-lipc-wait-event"), "missing command fails before capture");
  check(monitor.start(argv[1]), "start read-only subscriber");
  if (!std::strcmp(argv[2], "hold")) {
    // The Python check kills this process and verifies that the exec'd helper
    // stops writing its heartbeat without relying on PID namespace details.
    for (;;) pause();
  }
  const auto expected = static_cast<PowerEvent>(std::atoi(argv[2]));
  for (unsigned cycle = 0; cycle < 2; ++cycle) {
    PowerEvent event = PowerEvent::None;
    for (unsigned i = 0; i < 300 && event == PowerEvent::None; ++i) {
      event = monitor.poll();
      usleep(10000);
    }
    check(event == expected, "receive expected lifecycle event or helper failure");
    monitor.stop();
    monitor.stop();
    int status;
    check(waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD, "subscriber reaped and stop idempotent");
    if (cycle == 0) check(monitor.start(argv[1]), "subscriber restart resets stream state");
  }
  std::printf("power_checks failures=%u\n", failures);
  return failures ? 1 : 0;
}
