#include "KindlePower.h"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace kindle {
namespace {
// Match KOReader's stock CLI subscription on legacy Kindle firmware. A live
// wildcard listener is not proof that these named power events were subscribed.
constexpr const char* powerEvents =
    "goingToScreenSaver,outOfScreenSaver,exitingScreenSaver,charging,notCharging,wakeupFromSuspend,readyToSuspend";
bool closeOnExec(int fd) { return fcntl(fd, F_SETFD, FD_CLOEXEC) == 0; }
bool named(const char* line, const char* event) {
  const size_t length = std::strlen(event);
  return !std::strncmp(line, event, length) &&
         (!line[length] || line[length] == ' ' || line[length] == '\t' || line[length] == '\r');
}
uint64_t milliseconds() {
  timespec now{};
  clock_gettime(CLOCK_MONOTONIC, &now);
  return static_cast<uint64_t>(now.tv_sec) * 1000 + now.tv_nsec / 1000000;
}
}  // namespace

bool PowerMonitor::logState(const char* stage, const char* command) {
  int output[2];
  if (pipe(output) != 0) return false;
  if (!closeOnExec(output[0]) || !closeOnExec(output[1]) || fcntl(output[0], F_SETFL, O_NONBLOCK) != 0) {
    close(output[0]);
    close(output[1]);
    return false;
  }
  const pid_t parent = getpid();
  const pid_t child = fork();
  if (child == 0) {
    close(output[0]);
    if (prctl(PR_SET_PDEATHSIG, SIGKILL) == 0 && getppid() == parent && dup2(output[1], STDOUT_FILENO) >= 0) {
      close(output[1]);
      char* arguments[] = {const_cast<char*>(command), const_cast<char*>("com.lab126.powerd"),
                           const_cast<char*>("state"), nullptr};
      execve(command, arguments, environ);
    }
    _exit(127);
  }
  close(output[1]);
  if (child < 0) {
    close(output[0]);
    return false;
  }
  char value[128]{};
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
      if (used + 1 < sizeof(value))
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
  const int exitCode = result == child && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  std::printf("power_state stage=%s value=%s exit=%d timeout=%d truncated=%d\n", stage, used ? value : "unavailable",
              exitCode, timedOut, overflow);
  return exitCode == 0 && !timedOut && !overflow && used;
}

bool PowerMonitor::start(const char* command) {
  if (fd_ >= 0 || child_ > 0 || !command || !*command) return false;
  failed_ = false;
  lines_ = 0;
  used_ = 0;
  bytes_ = 0;
  overflow_ = false;
  int output[2], execution[2];
  // A stock command can line-buffer stdout only when isatty() succeeds. Giving
  // it a terminal prevents an isolated event from waiting for a pipe buffer to
  // fill. Keep the terminal raw so bytes reach the existing line parser intact.
  output[0] = posix_openpt(O_RDWR | O_NOCTTY);
  if (output[0] < 0) {
    std::fprintf(stderr, "ERROR power PTY allocate errno=%d\n", errno);
    return false;
  }
  // Linux devpts creates the slave with its owner/mode. Legacy grantpt()
  // resolves the tty group through NSS, which can dlopen the Kindle's libc
  // into this static executable and abort. Verify the kernel's permissions
  // below instead of changing them or loading account/group modules.
  char terminal[128];
  const int nameError = ptsname_r(output[0], terminal, sizeof(terminal));
  if (nameError != 0 || unlockpt(output[0]) != 0) {
    std::fprintf(stderr, "ERROR power PTY identify/unlock errno=%d\n", nameError ? nameError : errno);
    close(output[0]);
    return false;
  }
  output[1] = open(terminal, O_RDWR | O_NOCTTY);
  termios attributes{};
  if (output[1] < 0 || tcgetattr(output[1], &attributes) != 0) {
    std::fprintf(stderr, "ERROR power PTY open/attributes errno=%d\n", errno);
    close(output[0]);
    if (output[1] >= 0) close(output[1]);
    return false;
  }
  struct stat metadata{};
  if (fstat(output[1], &metadata) != 0 || !S_ISCHR(metadata.st_mode) || metadata.st_uid != geteuid() ||
      (metadata.st_mode & (S_IRUSR | S_IWUSR)) != (S_IRUSR | S_IWUSR) ||
      (metadata.st_mode & (S_IRGRP | S_IROTH | S_IWOTH))) {
    std::fprintf(stderr, "ERROR power PTY owner/mode rejected; no permissions changed\n");
    close(output[0]);
    close(output[1]);
    return false;
  }
  cfmakeraw(&attributes);
  if (tcsetattr(output[1], TCSANOW, &attributes) != 0) {
    std::fprintf(stderr, "ERROR power PTY raw mode errno=%d\n", errno);
    close(output[0]);
    close(output[1]);
    return false;
  }
  if (pipe(execution) != 0) {
    close(output[0]);
    close(output[1]);
    return false;
  }
  if (!closeOnExec(output[0]) || !closeOnExec(output[1]) || !closeOnExec(execution[0]) || !closeOnExec(execution[1]) ||
      fcntl(output[0], F_SETFL, O_NONBLOCK) != 0) {
    close(output[0]);
    close(output[1]);
    close(execution[0]);
    close(execution[1]);
    return false;
  }
  const pid_t parent = getpid();
  child_ = fork();
  if (child_ == 0) {
    close(output[0]);
    close(execution[0]);
    // Only async-signal-safe operations between fork and exec. The subscriber
    // gets no framebuffer/input handles because these are close-on-exec.
    // The reader may be killed by the launcher's deadline. Linux 2.6.31
    // supports this death signal, so its subscriber cannot remain orphaned.
    if (prctl(PR_SET_PDEATHSIG, SIGKILL) == 0 && getppid() == parent && dup2(output[1], STDOUT_FILENO) >= 0) {
      close(output[1]);
      char* arguments[] = {const_cast<char*>(command),
                           const_cast<char*>("-m"),
                           const_cast<char*>("-s"),
                           const_cast<char*>("0"),
                           const_cast<char*>("com.lab126.powerd"),
                           const_cast<char*>(powerEvents),
                           nullptr};
      execve(command, arguments, environ);
    }
    const int failure = errno;
    const auto ignored = write(execution[1], &failure, sizeof(failure));
    (void)ignored;
    _exit(127);
  }
  close(output[1]);
  close(execution[1]);
  if (child_ < 0) {
    close(output[0]);
    close(execution[0]);
    return false;
  }
  fd_ = output[0];
  int failure = 0;
  ssize_t count;
  do {
    count = read(execution[0], &failure, sizeof(failure));
  } while (count < 0 && errno == EINTR);
  close(execution[0]);
  if (count != 0) {
    std::fprintf(stderr, "ERROR power subscriber exec failed errno=%d\n", failure);
    stop();
    return false;
  }
  std::printf("power_listener=started pid=%ld source=com.lab126.powerd transport=pty events=%s\n",
              static_cast<long>(child_), powerEvents);
  return true;
}

PowerEvent PowerMonitor::parse(const char* line) {
  if (!line) return PowerEvent::None;
  if (named(line, "goingToScreenSaver")) return PowerEvent::GoingToSleep;
  if (named(line, "readyToSuspend")) return PowerEvent::ReadyToSuspend;
  if (named(line, "outOfScreenSaver") || named(line, "wakeupFromSuspend") || named(line, "exitingScreenSaver"))
    return PowerEvent::Waking;
  return PowerEvent::None;
}

PowerEvent PowerMonitor::poll() {
  if (fd_ < 0 || failed_) return PowerEvent::ListenerFailed;
  // Bound work per input tick even if another powerd source floods the pipe.
  for (unsigned i = 0; i < 512; ++i) {
    char byte;
    const ssize_t count = read(fd_, &byte, 1);
    if (count < 0 && (errno == EAGAIN || errno == EINTR)) return PowerEvent::None;
    if (count <= 0) {
      failed_ = true;
      std::fprintf(stderr, "ERROR power subscriber closed or read failed\n");
      return PowerEvent::ListenerFailed;
    }
    ++bytes_;
    if (byte == '\n') {
      line_[used_] = '\0';
      ++lines_;
      if (overflow_)
        std::fprintf(stderr, "power_raw=oversized-discarded\n");
      else
        std::printf("power_raw=%s\n", line_);
      const auto event = overflow_ ? PowerEvent::None : parse(line_);
      if (event != PowerEvent::None) std::printf("power_event=%s\n", line_);
      used_ = 0;
      overflow_ = false;
      if (event != PowerEvent::None) return event;
    } else if (used_ + 1 < sizeof(line_) && !overflow_) {
      line_[used_++] = byte;
    } else {
      overflow_ = true;
    }
  }
  return PowerEvent::None;
}

void PowerMonitor::stop() {
  if (fd_ >= 0) {
    close(fd_);
    fd_ = -1;
  }
  if (child_ <= 0) return;
  // waitpid confirms ownership; never signal a PID that was already reaped.
  int status = 0;
  pid_t result;
  do {
    result = waitpid(child_, &status, WNOHANG);
  } while (result < 0 && errno == EINTR);
  if (result == 0) {
    kill(child_, SIGTERM);
    for (unsigned i = 0; i < 20; ++i) {
      do {
        result = waitpid(child_, &status, WNOHANG);
      } while (result < 0 && errno == EINTR);
      if (result != 0) break;
      usleep(10000);
    }
    if (result == 0) {
      kill(child_, SIGKILL);
      do {
        result = waitpid(child_, &status, 0);
      } while (result < 0 && errno == EINTR);
    }
  }
  std::printf("power_listener=stopped reaped=%d lines_received=%u bytes_received=%u partial_bytes=%zu\n",
              result == child_, lines_, bytes_, used_);
  child_ = -1;
}
}  // namespace kindle
