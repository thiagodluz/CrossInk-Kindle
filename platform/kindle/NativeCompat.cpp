#include <sys/sysinfo.h>

#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <new>

#include "compat/Arduino.h"
#include "compat/Logging.h"
#include "compat/freertos/semphr.h"
#include "compat/freertos/task.h"

struct KindleSemaphore {
  pthread_mutex_t mutex;
  pthread_mutex_t metadata = PTHREAD_MUTEX_INITIALIZER;
  TaskHandle_t owner = nullptr;
  unsigned depth = 0;
  bool recursive = false;
};
namespace {
thread_local uint8_t identity;
}
TaskHandle_t xTaskGetCurrentTaskHandle() { return &identity; }
static SemaphoreHandle_t createMutex(bool recursive) {
  auto* m = new (std::nothrow) KindleSemaphore;
  if (!m) return nullptr;
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  m->recursive = recursive;
  pthread_mutexattr_settype(&attr, recursive ? PTHREAD_MUTEX_RECURSIVE : PTHREAD_MUTEX_NORMAL);
  const int result = pthread_mutex_init(&m->mutex, &attr);
  pthread_mutexattr_destroy(&attr);
  if (result) {
    delete m;
    return nullptr;
  }
  return m;
}
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() { return createMutex(true); }
SemaphoreHandle_t xSemaphoreCreateMutex() { return createMutex(false); }
BaseType_t xSemaphoreTake(SemaphoreHandle_t m, TickType_t timeout) {
  if (!m) return pdFALSE;
  pthread_mutex_lock(&m->metadata);
  const bool repeated = !m->recursive && m->owner == xTaskGetCurrentTaskHandle();
  pthread_mutex_unlock(&m->metadata);
  if (repeated) return pdFALSE;
  int status;
  if (timeout == portMAX_DELAY)
    status = pthread_mutex_lock(&m->mutex);
  else if (timeout == 0)
    status = pthread_mutex_trylock(&m->mutex);
  else {
    // PW1 glibc has no pthread_mutex_clocklock. A bounded, sleeping try-lock
    // keeps finite waits monotonic instead of depending on wall-clock changes.
    const unsigned long start = millis();
    do {
      status = pthread_mutex_trylock(&m->mutex);
      if (status == 0 || millis() - start >= timeout) break;
      usleep(1000);
    } while (true);
  }
  if (status) return pdFALSE;
  pthread_mutex_lock(&m->metadata);
  m->owner = xTaskGetCurrentTaskHandle();
  ++m->depth;
  pthread_mutex_unlock(&m->metadata);
  return pdTRUE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t m) {
  if (!m) return pdFALSE;
  pthread_mutex_lock(&m->metadata);
  if (m->owner != xTaskGetCurrentTaskHandle() || !m->depth) {
    pthread_mutex_unlock(&m->metadata);
    return pdFALSE;
  }
  if (--m->depth == 0) m->owner = nullptr;
  pthread_mutex_unlock(&m->metadata);
  return pthread_mutex_unlock(&m->mutex) == 0 ? pdTRUE : pdFALSE;
}
TaskHandle_t xSemaphoreGetMutexHolder(SemaphoreHandle_t m) {
  if (!m) return nullptr;
  pthread_mutex_lock(&m->metadata);
  const auto owner = m->owner;
  pthread_mutex_unlock(&m->metadata);
  return owner;
}
void vSemaphoreDelete(SemaphoreHandle_t m) {
  if (!m) return;
  pthread_mutex_destroy(&m->mutex);
  pthread_mutex_destroy(&m->metadata);
  delete m;
}
void vTaskDelay(TickType_t ticks) { delay(ticks); }
void logPrintf(const char* level, const char* origin, const char* format, ...) {
  flockfile(stdout);
  std::fprintf(stdout, "[%s] [%s] ", level, origin);
  va_list args;
  va_start(args, format);
  std::vfprintf(stdout, format, args);
  va_end(args);
  funlockfile(stdout);
}
KindleMemoryHints ESP;
uint32_t KindleMemoryHints::getFreeHeap() const {
  struct sysinfo info{};
  if (sysinfo(&info) != 0) return 0;
  // Linux availability hint for old allocation policy; not a largest-block measurement.
  const uint64_t available = (static_cast<uint64_t>(info.freeram) + info.bufferram) * info.mem_unit;
  return static_cast<uint32_t>(available > UINT32_MAX ? UINT32_MAX : available);
}
void KindleMemoryHints::restart() const {
  LOG_ERR("PW1", "Application restart requested; exiting only this process");
  raise(SIGTERM);
}
