#include <fcntl.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "LauncherSession.h"

namespace {
volatile sig_atomic_t received = 0;
void signalReceived(int) { received = received + 1; }
void write(const std::string& path, const std::string& value) {
  std::ofstream file(path);
  file << value << '\n';
}
}  // namespace
int main(int argc, char** argv) {
  if (argc == 2 && !std::strcmp(argv[1], "--session-unlock")) return kindle::unlockSessionGuard() ? 0 : 1;
  if (argc >= 3 && !std::strcmp(argv[1], "--session-guard")) return kindle::guardSession(argv[2], argc - 3, argv + 3);
  const char* root = std::getenv("CROSSINK_ROOT");
  if (argc != 2 || std::strcmp(argv[1], "--notify-test") || !root) return 2;
  const std::string lock = std::string(root) + "/state/launch.lock/";
  std::string boot, stat, field;
  std::ifstream("/proc/sys/kernel/random/boot_id") >> boot;
  std::getline(std::ifstream("/proc/self/stat"), stat);
  std::istringstream fields(stat.substr(stat.rfind(')') + 2));
  for (unsigned i = 3; i <= 22; ++i) fields >> field;
  const auto start = std::strtoull(field.c_str(), nullptr, 10);
  const std::string proc = std::string(root) + "/proc";
  write(proc + "/sys/kernel/random/boot_id", boot);
  // Isolated proc fixtures handle hosts exposing a different /proc PID namespace.
  const std::string pidDirectory = proc + "/" + std::to_string(getpid());
  if (mkdir(pidDirectory.c_str(), 0775) != 0) return 1;
  write(pidDirectory + "/stat", stat);
  write(lock + "owner", std::to_string(getpid()));
  write(lock + "identity", boot + " " + std::to_string(start));
  if (!kindle::captureLauncher(proc.c_str())) return 1;
  const int guard = open((std::string(root) + "/state/operation.lock").c_str(), O_CREAT | O_RDWR, 0600);
  if (guard < 0) return 1;
  close(guard);
  if (!kindle::registerReader(proc.c_str())) return 1;
  long recordedPid = -1;
  unsigned long long recordedStart = 0;
  std::ifstream(lock + "reader") >> recordedPid >> recordedStart;
  if (recordedPid != getpid() || recordedStart != start) return 1;
  struct sigaction action{};
  action.sa_handler = signalReceived;
  sigemptyset(&action.sa_mask);
  sigaction(SIGUSR1, &action, nullptr);
  unsetenv("CROSSINK_LAUNCHER_PID");
  // A real intermediary proves that the production notifier uses the verified
  // session owner rather than requiring the launcher to be its direct parent.
  const pid_t child = fork();
  if (child == 0) {
    const pid_t grandchild = fork();
    if (grandchild == 0) _exit(kindle::notifyLauncher(SIGUSR1, proc.c_str()) ? 0 : 1);
    int status = 0;
    if (grandchild < 0 || waitpid(grandchild, &status, 0) != grandchild) _exit(2);
    _exit(WIFEXITED(status) ? WEXITSTATUS(status) : 3);
  }
  int status = 0;
  pid_t result;
  do {
    result = waitpid(child, &status, 0);
  } while (result < 0 && errno == EINTR);
  if (result != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0 || received != 1) return 1;
  const pid_t replacement = fork();
  if (replacement == 0) {
    for (;;) pause();
  }
  if (replacement < 0) return 1;
  const std::string replacementDirectory = proc + "/" + std::to_string(replacement);
  if (mkdir(replacementDirectory.c_str(), 0775) != 0) {
    kill(replacement, SIGKILL);
    waitpid(replacement, nullptr, 0);
    return 1;
  }
  write(replacementDirectory + "/stat", stat);
  write(lock + "owner", std::to_string(replacement));
  const bool replacementRejected = !kindle::notifyLauncher(SIGUSR1, proc.c_str()) &&
                                   !kindle::captureLauncher(proc.c_str()) && !kindle::registerReader(proc.c_str());
  kill(replacement, SIGKILL);
  waitpid(replacement, nullptr, 0);
  if (!replacementRejected) return 1;
  write(lock + "owner", std::to_string(getpid()));
  write(lock + "identity", "old-boot " + std::to_string(start));
  if (kindle::notifyLauncher(SIGUSR1, proc.c_str())) return 1;
  write(lock + "identity", boot + " " + std::to_string(start + 1));
  if (kindle::notifyLauncher(SIGUSR1, proc.c_str())) return 1;
  write(lock + "identity", boot + " " + std::to_string(start));
  write(lock + "owner", "1");
  if (kindle::notifyLauncher(SIGUSR1, proc.c_str())) return 1;
  write(lock + "owner", "not-a-pid");
  if (kindle::notifyLauncher(SIGUSR1, proc.c_str())) return 1;
  if (received != 1) return 1;
  std::puts("session_check verified-owner-intermediary-and-stale-identities=ok");
  return 0;
}
