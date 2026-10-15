#include <sys/mman.h>

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "KindleBackends.h"

namespace {
constexpr int framebufferFd = 42;
constexpr size_t width = 758, height = 1024, stride = 768;
std::array<uint8_t, stride * height> framebuffer;
unsigned refreshes = 0, failures = 0;
void check(bool passed, const char* what) {
  if (!passed) {
    ++failures;
    std::fprintf(stderr, "FAIL %s\n", what);
  }
}
}  // namespace

// Model only the PW1 framebuffer syscalls. Exercise the real Display methods,
// including restoration from destruction, while a separate writer paints sleep.
extern "C" int __wrap_open(const char* path, int, ...) {
  return std::strcmp(path, "/dev/fb0") == 0 ? framebufferFd : -1;
}
extern "C" int __wrap_close(int fd) { return fd == framebufferFd ? 0 : -1; }
extern "C" void* __wrap_mmap(void*, size_t length, int, int, int fd, off_t) {
  return fd == framebufferFd && length == framebuffer.size() ? framebuffer.data() : MAP_FAILED;
}
extern "C" int __wrap_munmap(void* memory, size_t) { return memory == framebuffer.data() ? 0 : -1; }
extern "C" int __wrap_ioctl(int fd, unsigned long request, ...) {
  if (fd != framebufferFd) return -1;
  va_list arguments;
  va_start(arguments, request);
  void* data = va_arg(arguments, void*);
  va_end(arguments);
  if (request == FBIOGET_FSCREENINFO) {
    auto& info = *static_cast<fb_fix_screeninfo*>(data);
    info = {};
    std::strcpy(info.id, "mxc_epdc_fb");
    info.line_length = stride;
    info.smem_len = framebuffer.size();
  } else if (request == FBIOGET_VSCREENINFO) {
    auto& info = *static_cast<fb_var_screeninfo*>(data);
    info = {};
    info.xres = width;
    info.yres = height;
    info.bits_per_pixel = 8;
    info.rotate = 3;
  } else if (request == 0x4048462e) {
    ++refreshes;
  } else if (request != 0x4004462f) {
    return -1;
  }
  return 0;
}

int main() {
  framebuffer.fill(20);
  {
    kindle::Display display;
    check(display.open(true), "open modeled PW1 framebuffer");
    std::fill_n(display.pixels(), width * height, 80);
    check(display.present(true), "draw reader frame");
    framebuffer.fill(160);  // Amazon paints after publishing the power event.
    display.handoffToSystem();
    check(display.restore(), "power handoff releases restoration ownership");
    check(!display.present(true), "late reader frame rejected after handoff");
  }
  check(refreshes == 1, "no refresh after power handoff, including destruction");
  check(std::all_of(framebuffer.begin(), framebuffer.end(), [](uint8_t pixel) { return pixel == 160; }),
        "Amazon sleep frame survives reader cleanup");

  framebuffer.fill(30);
  refreshes = 0;
  {
    kindle::Display display;
    check(display.open(true), "open normal-exit framebuffer");
    std::fill_n(display.pixels(), width * height, 90);
    check(display.present(true), "normal reader frame");
    check(display.restore(), "normal exit restores captured frame");
  }
  check(refreshes == 2, "normal exit restores exactly once");
  check(std::all_of(framebuffer.begin(), framebuffer.end(), [](uint8_t pixel) { return pixel == 30; }),
        "normal exit retains prior restore behavior");
  std::printf("display_checks failures=%u\n", failures);
  return failures ? 1 : 0;
}
