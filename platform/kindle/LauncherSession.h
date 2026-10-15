#pragma once

namespace kindle {
// Exec the shell with an exclusive kernel lock on descriptor 9. The shell
// explicitly unlocks it before waiting; cleanup/recovery reacquire it.
int guardSession(const char* runner, int count, char** arguments);
bool checkSessionGuard();
bool unlockSessionGuard();
bool captureLauncher(const char* identityProc = "/proc");
bool registerReader(const char* identityProc = "/proc");
bool notifyLauncher(int signal, const char* identityProc = "/proc");
void logLauncherIdentity(const char* identityProc = "/proc");
}  // namespace kindle
