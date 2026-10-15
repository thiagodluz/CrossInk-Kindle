#include "ProcessDescriptors.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstring>

namespace kindle {
bool closeInheritedDescriptors(int keep) {
  const int directory = open("/proc/self/fd", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (directory < 0) return false;
  // Raw getdents64 avoids allocating DIR state between fork and exec. This
  // fixed 256-byte kernel packet is independent of the process fd limit.
  char entries[256];
  bool ok = true;
  while (ok) {
    const long count = syscall(SYS_getdents64, directory, entries, sizeof(entries));
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      ok = count == 0;
      break;
    }
    size_t position = 0;
    while (position < static_cast<size_t>(count)) {
      constexpr size_t nameOffset = offsetof(dirent64, d_name);
      unsigned short length = 0;
      if (static_cast<size_t>(count) - position < nameOffset + 1) {
        ok = false;
        break;
      }
      std::memcpy(&length, entries + position + offsetof(dirent64, d_reclen), sizeof(length));
      if (length < nameOffset + 1 || length > static_cast<size_t>(count) - position) {
        ok = false;
        break;
      }
      const char* name = entries + position + nameOffset;
      int descriptor = 0;
      size_t index = 0;
      for (; index < length - nameOffset && name[index] >= '0' && name[index] <= '9'; ++index) {
        if (descriptor > (INT_MAX - 9) / 10) break;
        descriptor = descriptor * 10 + name[index] - '0';
      }
      if (index && index < length - nameOffset && name[index] == 0 && descriptor > STDERR_FILENO &&
          descriptor != directory && descriptor != keep)
        close(descriptor);
      position += length;
    }
  }
  close(directory);
  if (!ok && !errno) errno = EIO;
  return ok;
}
}  // namespace kindle
