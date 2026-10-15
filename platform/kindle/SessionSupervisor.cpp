#include "SessionSupervisor.h"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "LauncherSession.h"
#include "ProcessDescriptors.h"
#include "SessionHeartbeat.h"

namespace kindle {
namespace {
volatile sig_atomic_t stopped = 0;
void stop(int number) { stopped = number; }
int exitCode(int status) { return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status); }
bool recoverToken(const char* runner, const std::string& token) {
  const pid_t child = fork();
  if (child == 0) {
    char mode[] = "--recover-token";
    char* args[] = {mode, const_cast<char*>(token.c_str())};
    _exit(guardSession(runner, 2, args));
  }
  if (child < 0) return false;
  const auto deadline = monotonicMilliseconds() + 15000;
  int status = 0;
  for (;;) {
    const auto result = waitpid(child, &status, WNOHANG);
    if (result == child) return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (result < 0 && errno != EINTR) return false;
    if (monotonicMilliseconds() >= deadline) break;
    usleep(20000);
  }
  kill(child, SIGKILL);
  while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
  }
  return false;
}
}  // namespace
int superviseSession(const char* runner, const SupervisorLimits& limits) {
  const char* root = std::getenv("CROSSINK_ROOT");
  const std::string base = root ? root : "/mnt/us/crossink";
  if (base.empty() || base[0] != '/' || base + "/runner.sh" != runner) return 2;
  int pulse[2];
  if (pipe(pulse)) return 1;
  for (int fd : pulse) {
    if (fcntl(fd, F_SETFL, O_NONBLOCK) || fcntl(fd, F_SETFD, FD_CLOEXEC)) {
      close(pulse[0]);
      close(pulse[1]);
      return 1;
    }
  }
  // Keep the heartbeat writer distinct from the guard's reserved descriptor 9.
  if (pulse[1] == 9) {
    const int duplicate = fcntl(pulse[1], F_DUPFD_CLOEXEC, 10);
    if (duplicate < 0) {
      close(pulse[0]);
      close(pulse[1]);
      return 1;
    }
    close(pulse[1]);
    pulse[1] = duplicate;
  }
  // Publish before the single-threaded launcher fork.
  char descriptor[24];
  std::snprintf(descriptor, sizeof(descriptor), "%d", pulse[1]);
  if (setenv("CROSSINK_HEARTBEAT_FD", descriptor, 1)) {
    close(pulse[0]);
    close(pulse[1]);
    return 1;
  }
  struct sigaction handler{};
  handler.sa_handler = stop;
  sigemptyset(&handler.sa_mask);
  sigaction(SIGTERM, &handler, nullptr);
  sigaction(SIGINT, &handler, nullptr);
  // KUAL/shell dispositions must not auto-reap the child we supervise.
  signal(SIGCHLD, SIG_DFL);
  signal(SIGPIPE, SIG_IGN);
  // The native supervisor replaces the external runner. Relay existing KUAL/
  // test signals to its owned shell, whose traps implement the normal policy.
  sigaction(SIGUSR1, &handler, nullptr);
  sigaction(SIGUSR2, &handler, nullptr);
  const pid_t parent = getpid();
  const pid_t child = fork();
  if (!child) {
    close(pulse[0]);
    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    signal(SIGUSR1, SIG_DFL);
    signal(SIGUSR2, SIG_DFL);
    if (prctl(PR_SET_PDEATHSIG, SIGTERM) || getppid() != parent || fcntl(pulse[1], F_SETFD, 0) ||
        !closeInheritedDescriptors(pulse[1]))
      _exit(1);
    // This CLI forks before any threads or device acquisition.
    char mode[] = "--ui";
    char* args[] = {mode};
    _exit(guardSession(runner, 1, args));
  }
  close(pulse[1]);
  unsetenv("CROSSINK_HEARTBEAT_FD");
  if (child < 0) {
    close(pulse[0]);
    return 1;
  }
  const auto start = monotonicMilliseconds();
  uint64_t input = 0, render = 0, recovering = 0;
  bool fault = false, requested = false;
  std::string token;
  const char* identity = std::getenv("CROSSINK_IDENTITY_PROC_ROOT");
  if (!identity) identity = "/proc";
  std::printf("supervisor=started runner=%ld startup_ms=%u input_ms=%u render_ms=%u\n", static_cast<long>(child),
              limits.startupMs, limits.inputMs, limits.renderMs);
  for (;;) {
    int status = 0;
    const pid_t result = waitpid(child, &status, WNOHANG);
    if (result == child) {
      close(pulse[0]);
      // Also cover unexpected runner death. Never turn a failure into success
      // merely because its token-qualified restoration succeeded.
      if ((fault || exitCode(status)) && !token.empty() && !recoverToken(runner, token)) {
        std::fprintf(stderr, "supervisor=recovery-failed; session records retained\n");
        return fault || !exitCode(status) ? 1 : exitCode(status);
      }
      if (fault) return 1;
      return exitCode(status);
    }
    if (result < 0 && errno != EINTR) {
      close(pulse[0]);
      return 1;
    }
    const auto now = monotonicMilliseconds();
    if (token.empty()) captureOwnedSession(child, token, identity);
    pollfd event{pulse[0], POLLIN, 0};
    poll(&event, 1, 100);
    char bytes[64];
    const auto count = read(pulse[0], bytes, sizeof(bytes));
    for (ssize_t i = 0; i < count; ++i) {
      if (bytes[i] == 'I') input = now;
      if (bytes[i] == 'R') render = now;
    }
    const char* reason = nullptr;
    if ((!input || !render) && now - start >= limits.startupMs) reason = "startup";
    if (input && now - input >= limits.inputMs) reason = "input";
    if (render && now - render >= limits.renderMs) reason = "render";
    if (stopped && !requested && !fault) {
      requested = true;
      recovering = now;
      kill(child, stopped);
      kill(child, SIGCONT);
    }
    if (!fault && !requested && reason) {
      fault = true;
      recovering = now;
      std::printf("supervisor=timeout reason=%s runner=%ld\n", reason, static_cast<long>(child));
      kill(child, SIGALRM);
      kill(child, SIGCONT);
    }
    if ((fault || requested) && now - recovering >= limits.cleanupMs) {
      fault = true;
      kill(child, SIGKILL);
      // Reap on the next pass before token-qualified recovery.
    }
  }
}
}  // namespace kindle
