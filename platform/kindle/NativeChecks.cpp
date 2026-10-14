#include <Arduino.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalGPIO.h>
#include <HalStorage.h>
#include <Memory.h>
#include <freertos/task.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "RenderWorker.h"

namespace {
std::atomic<unsigned> failures{0};
void check(bool ok, const char* label) {
  if (!ok) {
    ++failures;
    std::fprintf(stderr, "FAIL %s\n", label);
  }
}
struct WorkerState {
  std::atomic<unsigned> count{0};
  std::atomic<bool> stopped{false};
  SemaphoreHandle_t mutex = xSemaphoreCreateRecursiveMutex();
  TaskHandle_t mainIdentity = xTaskGetCurrentTaskHandle();
};
void work(const kindle::UiJob& job, void* context) {
  auto& state = *static_cast<WorkerState*>(context);
  if (job.kind == kindle::UiJob::Kind::Stop) {
    state.stopped = true;
    return;
  }
  if (job.kind != kindle::UiJob::Kind::Touch) return;
  check(xTaskGetCurrentTaskHandle() != state.mainIdentity, "worker task identity differs from producer");
  check(xSemaphoreTakeRecursive(state.mutex, portMAX_DELAY) == pdTRUE, "worker owns recursive mutex");
  check(xSemaphoreTakeRecursive(state.mutex, 0) == pdTRUE, "recursive reacquisition");
  check(xSemaphoreGetMutexHolder(state.mutex) == xTaskGetCurrentTaskHandle(), "mutex holder is current renderer");
  check(xSemaphoreGiveRecursive(state.mutex) == pdTRUE && xSemaphoreGiveRecursive(state.mutex) == pdTRUE,
        "balanced recursive unlock");
  ++state.count;
  usleep(1000);
}
void storageChecks(const char* root) {
  check(kindle::setStorageRoot(root) && Storage.begin(), "anchor storage root");
  check(Storage.mkdir("/folder/nested"), "create nested private directories");
  check(Storage.writeFile("/folder/value.txt", String("before")), "first atomic file write");
  auto original = Storage.open("/folder/value.txt");
  check(Storage.writeFile("/folder/value.txt", String("after")), "replace via rename");
  char bytes[32] = {};
  check(original.read(bytes, 6) == 6 && !std::strcmp(bytes, "before"),
        "open reader sees old snapshot after atomic rename");
  original.close();
  check(!std::strcmp(Storage.readFile("/folder/value.txt").c_str(), "after"), "new reader sees replacement");
  check(!Storage.open("/../outside.txt"), "reject parent traversal");
  check(!Storage.open("/folder/../../outside.txt"), "reject nested traversal");
  const std::string parent = std::string(root).substr(0, std::string(root).find_last_of('/'));
  FILE* outside = std::fopen((parent + "/outside.txt").c_str(), "wb");
  if (outside) {
    std::fputs("outside", outside);
    std::fclose(outside);
  }
  check(outside != nullptr, "create outside fixture");
  check(::symlink("..", (std::string(root) + "/escape").c_str()) == 0, "create parent symlink fixture");
  check(!Storage.open("/escape/outside.txt"), "reject symlink directory escape");
  check(::symlink("../outside.txt", (std::string(root) + "/alias.txt").c_str()) == 0, "create file symlink fixture");
  check(!Storage.open("/alias.txt", O_WRONLY | O_TRUNC), "reject truncation through symlink");
  check(!Storage.rename("/escape/outside.txt", "/folder/stolen.txt"), "reject rename outside anchor");
  check(Storage.writeFile("/alias.txt", String("inside")), "atomic replace changes only inside symlink entry");
  outside = std::fopen((parent + "/outside.txt").c_str(), "rb");
  std::memset(bytes, 0, sizeof(bytes));
  if (outside) {
    check(std::fread(bytes, 1, 7, outside) == 7, "read outside fixture");
    std::fclose(outside);
  }
  check(!std::strcmp(bytes, "outside"), "outside file remains untouched");
  auto file = Storage.open("/folder/value.txt");
  check(file.seek64(2) && file.read() == 't' && file.seekCur(-1) && file.position() == 2, "seek and byte reads");
  file.close();
  auto folder = Storage.open("/folder");
  size_t children = 0;
  while (auto child = folder.openNextFile()) {
    ++children;
    child.close();
  }
  check(children == 2 && !folder.iterationFailed(), "directory iteration separates EOF from errors");
  folder.close();
  check(!Storage.remove("/"), "cannot remove storage root");
}
void drawingChecks(const char* root) {
  check(display.initialize(true), "preview display initialized");
  GfxRenderer renderer(display);
  renderer.begin();
  renderer.clearScreen();
  const int points[][2] = {{0, 0},    {757, 0},  {0, 1023},  {757, 1023}, {379, 512}, {41, 95},
                           {686, 68}, {57, 959}, {679, 963}, {388, 508},  {370, 942}};
  for (const auto& point : points) {
    kindle::Touch touch;
    touch.nx = point[0] / 757.0f;
    touch.ny = point[1] / 1023.0f;
    float nx, ny;
    int x, y;
    HalGPIO::canonicalTouch(touch, nx, ny);
    renderer.tapToLogical(nx, ny, x, y);
    check(x == point[0] && y == point[1], "canonical touch inverse matches renderer rotation");
    renderer.drawPixel(x, y, true);
  }
  const std::string path = std::string(root) + "/geometry.pgm";
  check(display.savePreview(path.c_str()), "save converted pixel plane");
  FILE* image = std::fopen(path.c_str(), "rb");
  if (image) {
    char header[32];
    check(std::fgets(header, sizeof(header), image) && std::fgets(header, sizeof(header), image) &&
              std::fgets(header, sizeof(header), image),
          "read PGM header");
    const auto start = std::ftell(image);
    for (const auto& point : points) {
      std::fseek(image, start + point[1] * 758 + point[0], SEEK_SET);
      check(std::fgetc(image) == 0, "rotated framebuffer pixel lands at touched coordinate");
    }
    std::fclose(image);
  } else
    check(false, "open pixel fixture");
  auto aligned = makeAlignedByteBufferNoThrow(79);
  check(aligned && reinterpret_cast<uintptr_t>(aligned.get()) % alignof(std::max_align_t) == 0,
        "old-glibc aligned allocation fallback");
}
void taskChecks() {
  WorkerState state;
  kindle::RenderWorker worker(work, &state);
  check(worker.start(), "start joinable worker");
  for (unsigned i = 0; i < 100; ++i) {
    kindle::UiJob job;
    job.kind = kindle::UiJob::Kind::Touch;
    const unsigned long deadline = millis() + 3000;
    while (!worker.enqueue(job) && millis() < deadline) usleep(1000);
  }
  const unsigned long deadline = millis() + 5000;
  while (state.count < 100 && millis() < deadline) usleep(1000);
  check(state.count == 100, "queued notifications have no lost wakeups");
  worker.stop();
  check(state.stopped, "cooperative stop callback finishes before join returns");
  check(!worker.enqueue({}), "closed worker rejects input");
  check(!worker.start(), "a stopped worker is not silently restarted");
  vSemaphoreDelete(state.mutex);
  auto normal = xSemaphoreCreateMutex();
  check(xSemaphoreTake(normal, 0) == pdTRUE && xSemaphoreTake(normal, 0) == pdFALSE, "ordinary mutex is not recursive");
  check(xSemaphoreGive(normal) == pdTRUE && xSemaphoreGive(normal) == pdFALSE, "mutex rejects unmatched unlock");
  vSemaphoreDelete(normal);
}
}  // namespace
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  storageChecks(argv[1]);
  drawingChecks(argv[1]);
  taskChecks();
  Storage.shutdown();
  std::printf("native_checks failures=%u\n", failures.load());
  return failures.load() ? 1 : 0;
}
