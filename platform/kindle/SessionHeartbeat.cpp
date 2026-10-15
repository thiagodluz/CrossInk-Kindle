#include "SessionHeartbeat.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>

namespace kindle {
uint64_t monotonicMilliseconds() {
  timespec now{};
  clock_gettime(CLOCK_MONOTONIC, &now);
  return static_cast<uint64_t>(now.tv_sec) * 1000 + now.tv_nsec / 1000000;
}
bool SessionHeartbeat::begin(char channel) {
  const char* value = std::getenv("CROSSINK_HEARTBEAT_FD");
  char* tail = nullptr;
  errno = 0;
  const long descriptor = value ? std::strtol(value, &tail, 10) : -1;
  struct stat info{};
  if (!value || errno || tail == value || *tail || descriptor < 3 || descriptor > INT_MAX ||
      fstat(static_cast<int>(descriptor), &info) || !S_ISFIFO(info.st_mode) ||
      (fcntl(static_cast<int>(descriptor), F_GETFL) & (O_NONBLOCK | O_ACCMODE)) != (O_NONBLOCK | O_WRONLY)) {
    std::fprintf(stderr, "heartbeat=failed stage=descriptor channel=%c\n", channel);
    return false;
  }
  fd_ = static_cast<int>(descriptor);
  channel_ = channel;
  return pulse();
}
bool SessionHeartbeat::pulse() {
  const auto now = monotonicMilliseconds();
  if (last_ && now - last_ < 1000) return true;
  ssize_t sent;
  do {
    sent = write(fd_, &channel_, 1);
  } while (sent < 0 && errno == EINTR);
  if (sent == 1) {
    last_ = now;
    return true;
  }
  std::fprintf(stderr, "heartbeat=failed stage=write channel=%c errno=%d\n", channel_, errno);
  return false;
}
}  // namespace kindle
