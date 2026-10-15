#include "LauncherSession.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace kindle {
namespace {
long capturedOwner = -1;
unsigned long long capturedStart = 0;
std::string root() {
  const char* value = std::getenv("CROSSINK_ROOT");
  return value ? value : "/mnt/us/crossink";
}
bool readLine(const std::string& path, char* out, size_t capacity) {
  FILE* file = std::fopen(path.c_str(), "r");
  if (!file) return false;
  const bool ok = std::fgets(out, capacity, file) && (std::strchr(out, '\n') || std::feof(file));
  std::fclose(file);
  return ok;
}
bool startTime(long pid, unsigned long long& value, const char* identityProc) {
  // /proc stat can include spaces and ')' in comm; fields follow its last ')'.
  std::string text;
  FILE* file = std::fopen((std::string(identityProc) + "/" + std::to_string(pid) + "/stat").c_str(), "r");
  if (!file) return false;
  char piece[128];
  while (std::fgets(piece, sizeof(piece), file) && text.size() < 4096) text += piece;
  std::fclose(file);
  const auto end = text.rfind(')');
  if (end == std::string::npos) return false;
  const char* cursor = text.c_str() + end + 1;
  for (unsigned field = 3; field <= 22; ++field) {
    while (*cursor == ' ') ++cursor;
    if (!*cursor) return false;
    if (field == 22) {
      char* tail = nullptr;
      errno = 0;
      value = std::strtoull(cursor, &tail, 10);
      return !errno && tail != cursor && (*tail == ' ' || *tail == '\n') && value != 0;
    }
    while (*cursor && *cursor != ' ') ++cursor;
  }
  return false;
}
bool owner(long& pid, unsigned long long& started, const char* identityProc = "/proc") {
  const std::string lock = root() + "/state/launch.lock/";
  char record[128], boot[64], actualBoot[64], tail;
  if (!readLine(lock + "owner", record, sizeof(record)) || std::sscanf(record, "%ld %c", &pid, &tail) != 1 ||
      pid <= 1 || pid > INT_MAX)
    return false;
  if (!readLine(lock + "identity", record, sizeof(record)) ||
      std::sscanf(record, "%63s %llu %c", boot, &started, &tail) != 2 ||
      !readLine(std::string(identityProc) + "/sys/kernel/random/boot_id", actualBoot, sizeof(actualBoot)))
    return false;
  actualBoot[std::strcspn(actualBoot, "\r\n")] = 0;
  unsigned long long current = 0;
  return !std::strcmp(boot, actualBoot) && startTime(pid, current, identityProc) && current == started;
}
}  // namespace

int guardSession(const char* runner, int count, char** arguments) {
  const auto base = root();
  if (base.empty() || base[0] != '/' || base + "/runner.sh" != runner) return 2;
  const auto state = base + "/state";
  if (mkdir(state.c_str(), 0775) != 0 && errno != EEXIST) return 1;
  // Never unlink this inode: unlinking a held flock would allow a second lock.
  const int fd = open((state + "/operation.lock").c_str(), O_CREAT | O_RDWR | O_NOFOLLOW, 0600);
  if (fd < 0) return 1;
  struct stat info{};
  if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_uid != geteuid() || info.st_nlink != 1) {
    close(fd);
    return 1;
  }
  int locked = -1;
  const bool cleanup = count > 0 && !std::strcmp(arguments[0], "--cleanup");
  // Cleanup must not be lost merely because a duplicate launch briefly owns
  // the guard. Interactive recovery stays nonblocking; cleanup retries boundedly.
  for (unsigned attempt = 0; attempt < (cleanup ? 100U : 1U); ++attempt) {
    locked = flock(fd, LOCK_EX | LOCK_NB);
    if (locked == 0 || (errno != EWOULDBLOCK && errno != EINTR)) break;
    if (cleanup) usleep(100000);
  }
  if (locked != 0) {
    std::fprintf(stderr, "session_guard=busy; another setup/recovery is in progress\n");
    close(fd);
    return 75;
  }
  if ((fd != 9 && dup2(fd, 9) < 0) || fcntl(9, F_SETFD, 0) != 0) {
    close(fd);
    return 1;
  }
  if (fd != 9) close(fd);
  // This one-time vector survives only until exec, never in an input/render loop.
  std::vector<char*> command{const_cast<char*>("/bin/sh"), const_cast<char*>(runner), const_cast<char*>("--guarded")};
  for (int i = 0; i < count; ++i) command.push_back(arguments[i]);
  command.push_back(nullptr);
  execve("/bin/sh", command.data(), environ);
  std::perror("exec guarded launcher");
  close(9);
  return 1;
}
void logLauncherIdentity(const char* identityProc) {
  const char* raw = std::getenv("CROSSINK_LAUNCHER_PID");
  long kernelParent = -1;
  FILE* status = std::fopen("/proc/self/status", "r");
  if (status) {
    char line[128];
    while (std::fgets(line, sizeof(line), status))
      if (std::sscanf(line, "PPid: %ld", &kernelParent) == 1) break;
    std::fclose(status);
  }
  long pid = -1;
  unsigned long long started = 0;
  const bool valid = owner(pid, started, identityProc);
  std::printf("launcher_identity env=%.32s ppid=%ld proc_ppid=%ld owner=%ld start=%llu valid=%d\n",
              raw ? raw : "(unset)", static_cast<long>(getppid()), kernelParent, pid, started, valid);
}
bool checkSessionGuard() {
  struct stat held{}, current{};
  const int fd = open((root() + "/state/operation.lock").c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0) return false;
  const bool ok = fstat(9, &held) == 0 && fstat(fd, &current) == 0 && S_ISREG(held.st_mode) &&
                  held.st_uid == geteuid() && held.st_nlink == 1 && held.st_dev == current.st_dev &&
                  held.st_ino == current.st_ino && flock(9, LOCK_EX | LOCK_NB) == 0;
  close(fd);
  return ok;
}
bool notifyLauncher(int signal, const char* identityProc) {
  if (signal != SIGUSR1 && signal != SIGUSR2) return false;
  long pid = -1;
  unsigned long long started = 0;
  if (!owner(pid, started, identityProc) || pid != capturedOwner || started != capturedStart) {
    std::fprintf(stderr, "launcher_signal=%d sent=0 reason=invalid-session-identity\n", signal);
    logLauncherIdentity(identityProc);
    return false;
  }
  const bool sent = kill(static_cast<pid_t>(pid), signal) == 0;
  std::printf("launcher_signal=%d sent=%d owner=%ld start=%llu\n", signal, sent, pid, started);
  return sent;
}
bool captureLauncher(const char* identityProc) {
  long pid = -1;
  unsigned long long started = 0;
  const bool valid = owner(pid, started, identityProc);
  logLauncherIdentity(identityProc);
  if (!valid) return false;
  if (capturedOwner != -1) return capturedOwner == pid && capturedStart == started;
  capturedOwner = pid;
  capturedStart = started;
  return true;
}
}  // namespace kindle
