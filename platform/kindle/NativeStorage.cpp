#define HAL_STORAGE_IMPL
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstring>
#include <new>

#include "compat/HalStorage.h"
#include "compat/Logging.h"

namespace {
int rootFd = -1;
std::string rootPath = "/mnt/us/crossink/data";
unsigned temporaryCounter = 0;
struct Fd {
  int value = -1;
  explicit Fd(int fd = -1) : value(fd) {}
  ~Fd() {
    if (value >= 0) ::close(value);
  }
  Fd(const Fd&) = delete;
  Fd& operator=(const Fd&) = delete;
};
bool components(const char* path, std::vector<std::string>& parts) {
  if (!path || std::strlen(path) > 4096) return false;
  const char* start = path;
  while (*start) {
    while (*start == '/') ++start;
    const char* end = start;
    while (*end && *end != '/') ++end;
    if (end != start) {
      std::string part(start, end);
      if (part == ".." || part.size() > NAME_MAX || parts.size() >= 128) return false;
      if (part != ".") parts.push_back(std::move(part));
    }
    start = end;
  }
  return true;
}
int openParent(const char* path, std::string& leaf, bool createParents = false) {
  std::vector<std::string> parts;
  if (rootFd < 0 || !components(path, parts)) {
    errno = EINVAL;
    return -1;
  }
  int fd = dup(rootFd);
  if (fd < 0) return -1;
  fcntl(fd, F_SETFD, FD_CLOEXEC);
  for (size_t i = 0; i + 1 < parts.size(); ++i) {
    if (createParents && mkdirat(fd, parts[i].c_str(), 0775) != 0 && errno != EEXIST) {
      ::close(fd);
      return -1;
    }
    const int next = openat(fd, parts[i].c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    ::close(fd);
    fd = next;
    if (fd < 0) return -1;
  }
  leaf = parts.empty() ? "." : parts.back();
  return fd;
}
std::string logicalPath(const char* path) {
  std::vector<std::string> parts;
  if (!components(path, parts)) return {};
  std::string result;
  for (const auto& p : parts) result += "/" + p;
  return result.empty() ? "/" : result;
}
bool writeAll(int fd, const void* data, size_t count) {
  const auto* bytes = static_cast<const uint8_t*>(data);
  size_t done = 0;
  while (done < count) {
    const auto n = ::write(fd, bytes + done, count - done);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) return false;
    done += n;
  }
  return true;
}
uint32_t fatTime(time_t value) {
  tm fields{};
  if (!localtime_r(&value, &fields) || fields.tm_year + 1900 < 1980 || fields.tm_year + 1900 > 2107) return 0;
  const uint32_t date = ((fields.tm_year - 80) << 9) | ((fields.tm_mon + 1) << 5) | fields.tm_mday;
  const uint32_t time = (fields.tm_hour << 11) | (fields.tm_min << 5) | (fields.tm_sec / 2);
  return (date << 16) | time;
}
}  // namespace

class HalStorage::StorageLock {
 public:
  StorageLock() { xSemaphoreTake(instance.storageMutex, portMAX_DELAY); }
  ~StorageLock() { xSemaphoreGive(instance.storageMutex); }
};
class HalFile::Impl {
 public:
  int fd = -1;
  DIR* directory = nullptr;
  std::string path;
  explicit Impl(int descriptor, std::string logical) : fd(descriptor), path(std::move(logical)) {}
  ~Impl() {
    if (directory)
      closedir(directory);
    else if (fd >= 0)
      ::close(fd);
  }
};
void HalFile::ImplDeleter::operator()(Impl* ptr) const { delete ptr; }
HalFile::HalFile() = default;
HalFile::HalFile(ImplPtr file) : impl(std::move(file)) {}
HalFile::~HalFile() = default;
HalFile::HalFile(HalFile&&) = default;
HalFile& HalFile::operator=(HalFile&&) = default;
bool HalFile::isOpen() const { return impl && impl->fd >= 0; }
HalFile::operator bool() const { return isOpen(); }
bool HalFile::isDirectory() const { return isOpen() && impl->directory; }
bool HalFile::close() {
  HalStorage::StorageLock lock;
  impl.reset();
  return true;
}
uint64_t HalFile::fileSize64() {
  HalStorage::StorageLock lock;
  struct stat info{};
  return isOpen() && fstat(impl->fd, &info) == 0 ? info.st_size : 0;
}
size_t HalFile::fileSize() { return static_cast<size_t>(std::min<uint64_t>(fileSize64(), SIZE_MAX)); }
size_t HalFile::size() { return fileSize(); }
size_t HalFile::getName(char* name, size_t length) {
  HalStorage::StorageLock lock;
  if (!name || !length || !isOpen()) return 0;
  const auto pos = impl->path.find_last_of('/');
  const char* source = impl->path.c_str() + (pos == std::string::npos ? 0 : pos + 1);
  const size_t n = std::min(length - 1, std::strlen(source));
  std::memcpy(name, source, n);
  name[n] = 0;
  return n;
}
bool HalFile::seek64(uint64_t position) {
  HalStorage::StorageLock lock;
  return isOpen() && !isDirectory() && position <= INT64_MAX &&
         lseek(impl->fd, static_cast<off_t>(position), SEEK_SET) >= 0;
}
bool HalFile::seek(size_t pos) { return seek64(pos); }
bool HalFile::seekSet(size_t pos) { return seek64(pos); }
bool HalFile::seekCur(int64_t offset) {
  HalStorage::StorageLock lock;
  return isOpen() && !isDirectory() && lseek(impl->fd, offset, SEEK_CUR) >= 0;
}
size_t HalFile::position() const {
  HalStorage::StorageLock lock;
  const auto pos = isOpen() ? lseek(impl->fd, 0, SEEK_CUR) : -1;
  return pos < 0 ? 0 : static_cast<size_t>(pos);
}
int HalFile::available() const {
  HalStorage::StorageLock lock;
  struct stat info{};
  if (!isOpen() || isDirectory() || fstat(impl->fd, &info) != 0) return 0;
  const auto pos = lseek(impl->fd, 0, SEEK_CUR);
  return pos < 0 || pos >= info.st_size ? 0 : static_cast<int>(std::min<int64_t>(info.st_size - pos, INT_MAX));
}
int HalFile::read(void* buffer, size_t count) {
  HalStorage::StorageLock lock;
  if (!isOpen() || isDirectory() || count > INT_MAX) return -1;
  ssize_t n;
  do {
    n = ::read(impl->fd, buffer, count);
  } while (n < 0 && errno == EINTR);
  return static_cast<int>(n);
}
int HalFile::read() {
  uint8_t value;
  return read(&value, 1) == 1 ? value : -1;
}
size_t HalFile::write(const void* bytes, size_t count) {
  HalStorage::StorageLock lock;
  if (!isOpen() || isDirectory()) return 0;
  return writeAll(impl->fd, bytes, count) ? count : 0;
}
size_t HalFile::write(uint8_t b) { return write(&b, 1); }
bool HalFile::sync() {
  HalStorage::StorageLock lock;
  return isOpen() && fsync(impl->fd) == 0;
}
void HalFile::flush() { sync(); }
uint32_t HalFile::creationTime() { return 0; }  // No portable creation timestamp on PW1/FAT.
uint32_t HalFile::modificationTime() {
  HalStorage::StorageLock lock;
  struct stat s{};
  return isOpen() && fstat(impl->fd, &s) == 0 ? fatTime(s.st_mtime) : 0;
}
bool HalFile::rename(const char* path) {
  if (!isOpen() || !Storage.rename(impl->path.c_str(), path)) return false;
  impl->path = logicalPath(path);
  return true;
}
void HalFile::rewindDirectory() {
  HalStorage::StorageLock lock;
  if (isDirectory()) rewinddir(impl->directory);
  iterationFailed_ = false;
}
HalFile HalFile::openNextFile() {
  HalStorage::StorageLock lock;
  if (!isDirectory()) return {};
  errno = 0;
  while (auto* entry = readdir(impl->directory)) {
    if (!std::strcmp(entry->d_name, ".") || !std::strcmp(entry->d_name, "..")) continue;
    const std::string path = (impl->path == "/" ? "" : impl->path) + "/" + entry->d_name;
    auto child = Storage.open(path.c_str());
    if (child) return child;
    if (child.allocationFailed()) {
      allocationFailed_ = true;
      iterationFailed_ = true;
      return {};
    }
    errno = 0;  // Unusable entries (including symlinks) are deliberately skipped.
  }
  iterationFailed_ = errno != 0;
  return {};
}

HalStorage HalStorage::instance;
HalStorage::HalStorage() : storageMutex(xSemaphoreCreateRecursiveMutex()) {}
HalStorage::~HalStorage() {
  shutdown();
  vSemaphoreDelete(storageMutex);
}
namespace kindle {
bool setStorageRoot(const char* root) {
  if (!root || root[0] != '/' || rootFd >= 0) return false;
  rootPath = root;
  return true;
}
}  // namespace kindle
bool HalStorage::begin() {
  if (!storageMutex) return false;
  StorageLock lock;
  if (rootFd >= 0) return true;
  if (::mkdir(rootPath.c_str(), 0775) != 0 && errno != EEXIST) {
    LOG_ERR("STO", "Cannot create application data root: %s", std::strerror(errno));
    return false;
  }
  rootFd = ::open(rootPath.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  initialized = rootFd >= 0;
  if (!initialized) LOG_ERR("STO", "Cannot open application data root: %s", std::strerror(errno));
  return initialized;
}
bool HalStorage::ready() const { return initialized; }
void HalStorage::shutdown() {
  StorageLock lock;
  if (rootFd >= 0) ::close(rootFd);
  rootFd = -1;
  initialized = false;
}
HalFile HalStorage::open(const char* path, oflag_t flags) {
  StorageLock lock;
  HalFile result;
  std::string leaf;
  Fd parent(openParent(path, leaf));
  if (parent.value < 0) return result;
  const int fd = openat(parent.value, leaf.c_str(), flags | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK, 0664);
  if (fd < 0) return result;
  struct stat info{};
  if (fstat(fd, &info) != 0 || (!S_ISREG(info.st_mode) && !S_ISDIR(info.st_mode))) {
    ::close(fd);
    return result;
  }
  auto* file = new (std::nothrow) HalFile::Impl(fd, logicalPath(path));
  if (!file) {
    ::close(fd);
    result.allocationFailed_ = true;
    return result;
  }
  result.impl.reset(file);
  if (S_ISDIR(info.st_mode)) {
    file->directory = fdopendir(fd);
    if (!file->directory) {
      result.impl.reset();
      return result;
    }
  }
  return result;
}
bool HalStorage::mkdir(const char* path, bool parents) {
  StorageLock lock;
  std::string leaf;
  Fd parent(openParent(path, leaf, parents));
  if (parent.value < 0) return false;
  if (mkdirat(parent.value, leaf.c_str(), 0775) != 0 && errno != EEXIST) return false;
  Fd folder(openat(parent.value, leaf.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
  return folder.value >= 0;
}
bool HalStorage::ensureDirectoryExists(const char* path) { return mkdir(path, true); }
bool HalStorage::exists(const char* path) {
  auto file = open(path);
  const bool found = file.isOpen();
  file.close();
  return found;
}
bool HalStorage::remove(const char* path) {
  StorageLock lock;
  std::string leaf;
  Fd parent(openParent(path, leaf));
  if (parent.value < 0 || leaf == ".") return false;
  struct stat info{};
  if (fstatat(parent.value, leaf.c_str(), &info, AT_SYMLINK_NOFOLLOW) != 0 || !S_ISREG(info.st_mode)) return false;
  return unlinkat(parent.value, leaf.c_str(), 0) == 0;
}
bool HalStorage::rmdir(const char* path) {
  StorageLock lock;
  std::string leaf;
  Fd parent(openParent(path, leaf));
  return parent.value >= 0 && leaf != "." && unlinkat(parent.value, leaf.c_str(), AT_REMOVEDIR) == 0;
}
bool HalStorage::rename(const char* from, const char* to) {
  StorageLock lock;
  std::string a, b;
  Fd pa(openParent(from, a)), pb(openParent(to, b));
  if (pa.value < 0 || pb.value < 0 || a == "." || b == ".") return false;
  struct stat info{};
  if (fstatat(pa.value, a.c_str(), &info, AT_SYMLINK_NOFOLLOW) != 0 || S_ISLNK(info.st_mode)) return false;
  if (fstatat(pb.value, b.c_str(), &info, AT_SYMLINK_NOFOLLOW) == 0 && S_ISLNK(info.st_mode)) return false;
  return renameat(pa.value, a.c_str(), pb.value, b.c_str()) == 0;
}
bool HalStorage::writeFile(const char* path, const String& text) {
  StorageLock lock;
  std::string leaf;
  Fd parent(openParent(path, leaf));
  if (parent.value < 0 || leaf == ".") return false;
  const std::string temporary = ".ci-write-" + std::to_string(getpid()) + "-" + std::to_string(++temporaryCounter);
  Fd file(openat(parent.value, temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0664));
  if (file.value < 0) return false;
  bool ok = writeAll(file.value, text.c_str(), text.length()) && fsync(file.value) == 0;
  if (ok) ok = renameat(parent.value, temporary.c_str(), parent.value, leaf.c_str()) == 0;
  if (!ok) unlinkat(parent.value, temporary.c_str(), 0);
  // Some user-store filesystems don't implement directory fsync; the atomic
  // rename has already succeeded, so only the file fsync is mandatory here.
  if (ok) fsync(parent.value);
  return ok;
}
std::vector<String> HalStorage::listFiles(const char* path, int limit) {
  std::vector<String> files;
  if (limit <= 0) return files;
  auto folder = open(path);
  while (files.size() < static_cast<size_t>(limit)) {
    auto entry = folder.openNextFile();
    if (!entry) break;
    char name[NAME_MAX + 1];
    entry.getName(name, sizeof(name));
    files.emplace_back(name);
    entry.close();
  }
  folder.close();
  return files;
}
size_t HalStorage::readFileToBuffer(const char* path, char* buffer, size_t size, size_t max) {
  if (!buffer || !size) return 0;
  auto file = open(path);
  const auto n = file.read(buffer, std::min(size - 1, max ? max : size - 1));
  file.close();
  const size_t count = n < 0 ? 0 : n;
  buffer[count] = 0;
  return count;
}
String HalStorage::readFile(const char* path) {
  auto file = open(path);
  const size_t count = file.size();
  if (!file || count > 1024 * 1024) {
    file.close();
    return {};
  }
  auto* buffer = new (std::nothrow) char[count + 1];
  if (!buffer) {
    file.close();
    return {};
  }
  const int n = file.read(buffer, count);
  buffer[n < 0 ? 0 : n] = 0;
  String text(buffer);
  delete[] buffer;
  file.close();
  return text;
}
bool HalStorage::readFileToStream(const char* path, Print& out, size_t chunk) {
  auto file = open(path);
  if (!file) return false;
  auto* bytes = new (std::nothrow) uint8_t[std::min<size_t>(chunk ? chunk : 256, 4096)];
  if (!bytes) {
    file.close();
    return false;
  }
  bool ok = true;
  const size_t capacity = std::min<size_t>(chunk ? chunk : 256, 4096);
  while (true) {
    int n = file.read(bytes, capacity);
    if (n <= 0) {
      ok = n == 0;
      break;
    }
    if (out.write(bytes, n) != static_cast<size_t>(n)) {
      ok = false;
      break;
    }
  }
  delete[] bytes;
  file.close();
  return ok;
}
bool HalStorage::openFileForRead(const char* module, const char* path, HalFile& file) {
  file = open(path, O_RDONLY);
  if (file) return true;
  LOG_ERR(module, "Cannot open for read: %s", path);
  return false;
}
bool HalStorage::openFileForWrite(const char* module, const char* path, HalFile& file) {
  file = open(path, O_WRONLY | O_CREAT | O_TRUNC);
  if (file) return true;
  LOG_ERR(module, "Cannot open for write: %s", path);
  return false;
}
bool HalStorage::openFileForRead(const char* m, const std::string& p, HalFile& f) {
  return openFileForRead(m, p.c_str(), f);
}
bool HalStorage::openFileForRead(const char* m, const String& p, HalFile& f) {
  return openFileForRead(m, p.c_str(), f);
}
bool HalStorage::openFileForWrite(const char* m, const std::string& p, HalFile& f) {
  return openFileForWrite(m, p.c_str(), f);
}
bool HalStorage::openFileForWrite(const char* m, const String& p, HalFile& f) {
  return openFileForWrite(m, p.c_str(), f);
}
bool HalStorage::removeDir(const char* path) {
  auto folder = open(path);
  if (!folder.isDirectory()) {
    folder.close();
    return false;
  }
  while (auto entry = folder.openNextFile()) {
    char name[NAME_MAX + 1];
    entry.getName(name, sizeof(name));
    const bool dir = entry.isDirectory();
    entry.close();
    const std::string child = logicalPath(path) + "/" + name;
    if (!(dir ? removeDir(child.c_str()) : remove(child.c_str()))) {
      folder.close();
      return false;
    }
  }
  const bool ok = !folder.iterationFailed();
  folder.close();
  return ok && rmdir(path);
}
uint64_t HalStorage::totalBytes() const {
  struct statvfs info{};
  return rootFd >= 0 && fstatvfs(rootFd, &info) == 0 ? static_cast<uint64_t>(info.f_blocks) * info.f_frsize : 0;
}
uint64_t HalStorage::usedBytes() {
  struct statvfs info{};
  return rootFd >= 0 && fstatvfs(rootFd, &info) == 0
             ? static_cast<uint64_t>(info.f_blocks - info.f_bfree) * info.f_frsize
             : 0;
}
void HalStorage::installDateTimeCallback(const uint8_t*) {}
bool HalStorage::beginUsbDrive() { return false; }
bool HalStorage::disconnectUsbDriveHost() { return false; }
void HalStorage::endUsbDrive() {}
UsbDriveState HalStorage::usbDriveState() const { return UsbDriveState::Unsupported; }
bool HalStorage::usbDriveHostSuspended() const { return false; }
