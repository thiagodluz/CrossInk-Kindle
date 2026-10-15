#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "SessionHeartbeat.h"
#include "SessionSupervisor.h"

int main(int argc, char** argv) {
  if (argc == 3 && !std::strcmp(argv[1], "--supervise"))
    return kindle::superviseSession(argv[2], {3000, 1600, 1800, 1000});
  if (argc != 1) return 2;
  kindle::SessionHeartbeat heartbeat;
  unsetenv("CROSSINK_HEARTBEAT_FD");
  if (heartbeat.begin('I')) return 1;
  setenv("CROSSINK_HEARTBEAT_FD", "9junk", 1);
  if (heartbeat.begin('I')) return 1;
  int descriptors[2];
  if (pipe(descriptors)) return 1;
  fcntl(descriptors[0], F_SETFL, O_NONBLOCK);
  char value[24];
  std::snprintf(value, sizeof(value), "%d", descriptors[1]);
  setenv("CROSSINK_HEARTBEAT_FD", value, 1);
  if (heartbeat.begin('I')) return 1;  // Blocking descriptors must be rejected.
  fcntl(descriptors[1], F_SETFL, O_NONBLOCK);
  if (!heartbeat.begin('I')) return 1;
  char byte;
  if (read(descriptors[0], &byte, 1) != 1 || byte != 'I') return 1;
  if (!heartbeat.pulse() || read(descriptors[0], &byte, 1) >= 0) return 1;
  signal(SIGPIPE, SIG_IGN);
  close(descriptors[0]);
  usleep(1100000);
  if (heartbeat.pulse()) return 1;
  close(descriptors[1]);
  std::puts("heartbeat_check descriptor-throttle-and-supervisor-loss=ok");
  return 0;
}
