#include "KindlePower.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

namespace kindle {
namespace {
bool closeOnExec(int fd) { return fcntl(fd, F_SETFD, FD_CLOEXEC) == 0; }
bool named(const char* line, const char* event) {
  const size_t length = std::strlen(event);
  return !std::strncmp(line, event, length) &&
         (!line[length] || line[length] == ' ' || line[length] == '\t' || line[length] == '\r');
}
}  // namespace

bool PowerMonitor::start(const char* command) {
  if (fd_ >= 0 || child_ > 0 || !command || !*command) return false;
  failed_ = false;
  used_ = 0;
  overflow_ = false;
  int output[2], execution[2];
  if (pipe(output) != 0) return false;
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
                           const_cast<char*>("*"),
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
  std::printf("power_listener=started pid=%ld source=com.lab126.powerd events=all\n", static_cast<long>(child_));
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
    if (byte == '\n') {
      line_[used_] = '\0';
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
  std::printf("power_listener=stopped reaped=%d\n", result == child_);
  child_ = -1;
}
}  // namespace kindle
