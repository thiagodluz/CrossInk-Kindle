#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "DiagnosticStrings.h"
#include "KindleBackends.h"

namespace {
volatile sig_atomic_t interrupted = 0;
void interrupt(int) { interrupted = 1; }
double now() {
  timespec time{};
  if (clock_gettime(CLOCK_MONOTONIC, &time) != 0) return -1;
  return time.tv_sec + time.tv_nsec / 1e9;
}

// Original compact 5x7 diagnostic font: static, reusable, no font allocation.
constexpr uint8_t glyphs[][5] = {
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, {0, 0x42, 0x7f, 0x40, 0},       {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4b, 0x31}, {0x18, 0x14, 0x12, 0x7f, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3c, 0x4a, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03}, {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1e}, {0x7e, 0x11, 0x11, 0x11, 0x7e}, {0x7f, 0x49, 0x49, 0x49, 0x36},
    {0x3e, 0x41, 0x41, 0x41, 0x22}, {0x7f, 0x41, 0x41, 0x22, 0x1c}, {0x7f, 0x49, 0x49, 0x49, 0x41},
    {0x7f, 0x09, 0x09, 0x09, 0x01}, {0x3e, 0x41, 0x49, 0x49, 0x7a}, {0x7f, 0x08, 0x08, 0x08, 0x7f},
    {0, 0x41, 0x7f, 0x41, 0},       {0x20, 0x40, 0x41, 0x3f, 0x01}, {0x7f, 0x08, 0x14, 0x22, 0x41},
    {0x7f, 0x40, 0x40, 0x40, 0x40}, {0x7f, 0x02, 0x0c, 0x02, 0x7f}, {0x7f, 0x04, 0x08, 0x10, 0x7f},
    {0x3e, 0x41, 0x41, 0x41, 0x3e}, {0x7f, 0x09, 0x09, 0x09, 0x06}, {0x3e, 0x41, 0x51, 0x21, 0x5e},
    {0x7f, 0x09, 0x19, 0x29, 0x46}, {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7f, 0x01, 0x01},
    {0x3f, 0x40, 0x40, 0x40, 0x3f}, {0x1f, 0x20, 0x40, 0x20, 0x1f}, {0x7f, 0x20, 0x18, 0x20, 0x7f},
    {0x63, 0x14, 0x08, 0x14, 0x63}, {0x03, 0x04, 0x78, 0x04, 0x03}, {0x61, 0x51, 0x49, 0x45, 0x43}};
class Canvas {
 public:
  Canvas(uint8_t* pixels, int width, int height) : pixels_(pixels), width_(width), height_(height) {}
  void fill(uint8_t shade) { std::memset(pixels_, shade, static_cast<size_t>(width_) * height_); }
  void rect(int x, int y, int w, int h, uint8_t shade) {
    for (int row = y < 0 ? 0 : y; row < y + h && row < height_; ++row)
      for (int col = x < 0 ? 0 : x; col < x + w && col < width_; ++col)
        pixels_[static_cast<size_t>(row) * width_ + col] = shade;
  }
  void text(const char* value, int y, int scale = 3) {
    int x = (width_ - static_cast<int>(std::strlen(value)) * 6 * scale) / 2;
    for (const char* p = value; *p; ++p, x += 6 * scale) {
      int index = *p >= '0' && *p <= '9' ? *p - '0' : (*p >= 'A' && *p <= 'Z' ? *p - 'A' + 10 : -1);
      if (index < 0) continue;
      for (int col = 0; col < 5; ++col)
        for (int row = 0; row < 7; ++row)
          if (glyphs[index][col] & (1U << row)) rect(x + col * scale, y + row * scale, scale, scale, 0);
    }
  }
  void outline(int x, int y, int w, int h) {
    rect(x, y, w, 3, 0);
    rect(x, y + h - 3, w, 3, 0);
    rect(x, y, 3, h, 0);
    rect(x + w - 3, y, 3, h, 0);
  }
  void diagnostic() {
    fill(255);
    text(tr(STR_TITLE), height_ / 7, 5);
    text(tr(STR_SUBTITLE), height_ / 4);
    text(tr(STR_INSTRUCTION), height_ / 3);
    text(tr(STR_CENTER), height_ / 3 + 40);
    for (int i = 0; i < 16; ++i)
      rect(30 + (width_ - 60) * i / 16, height_ * 5 / 8, (width_ - 60) * (i + 1) / 16 - (width_ - 60) * i / 16,
           height_ / 16, static_cast<uint8_t>(i * 17));
    const int xs[] = {12, width_ - 102, 12, width_ - 102};
    const int ys[] = {12, 12, height_ - 102, height_ - 102};
    for (int i = 0; i < 4; ++i) {
      outline(xs[i], ys[i], 90, 90);
      rect(xs[i] + 35, ys[i] + 35, 20, 20, 0);
      // Corner labels positioned locally, without modifying shared text centering.
      for (int col = 0; col < 5; ++col)
        for (int row = 0; row < 7; ++row)
          if (glyphs[i + 1][col] & (1U << row)) rect(xs[i] + 8 + col * 2, ys[i] + 8 + row * 2, 2, 2, 0);
    }
    outline(width_ / 2 - 45, height_ / 2 - 45, 90, 90);
    for (int y = 0; y < height_ / 24; y += 12)
      for (int x = 0; x < width_ - 60; x += 12)
        if ((x / 12 + y / 12) % 2 == 0) rect(30 + x, height_ * 3 / 4 + y, 12, 12, 0);
    text(tr(STR_TIMEOUT), height_ - 150, 2);
    outline(width_ / 2 - 100, height_ - 115, 200, 80);
    text(tr(STR_EXIT), height_ - 91, 4);
  }

 private:
  uint8_t* pixels_;
  int width_, height_;
};

int preview(const char* path, bool landscape = false) {
  const int w = landscape ? 1024 : 758, h = landscape ? 758 : 1024;
  const size_t count = static_cast<size_t>(w) * h;
  auto* pixels = static_cast<uint8_t*>(std::malloc(count));
  if (!pixels) return 1;
  Canvas(pixels, w, h).diagnostic();
  FILE* output = std::fopen(path, "wb");
  bool ok = false;
  if (output) {
    ok = std::fprintf(output, "P5\n%d %d\n255\n", w, h) > 0 && std::fwrite(pixels, 1, count, output) == count;
    if (std::fclose(output) != 0) ok = false;
  }
  std::free(pixels);
  return ok ? 0 : 1;
}

int run(bool probe) {
  kindle::Input input;
  kindle::Display display;
  if (!input.open(!probe) || !display.open(!probe)) return 1;
  if (probe) {
    std::puts("probe=ok no pixels written, input not grabbed");
    return 0;
  }
  struct sigaction handler{};
  handler.sa_handler = interrupt;
  sigemptyset(&handler.sa_mask);
  sigaction(SIGTERM, &handler, nullptr);
  sigaction(SIGINT, &handler, nullptr);
  sigaction(SIGALRM, &handler, nullptr);
  alarm(90);
  const auto& layout = display.layout();
  Canvas canvas(display.pixels(), layout.width, layout.height);
  // Full-screen black and white establish the update path before the test UI.
  canvas.fill(0);
  if (!display.present(true)) return 1;
  usleep(500000);
  canvas.fill(255);
  if (!display.present(true)) return 1;
  usleep(500000);
  canvas.diagnostic();
  if (!display.present(true)) return 1;
  std::puts("diagnostic=started timeout_seconds=90 touch_mapping=raw-normalized no orientation correction assumed");
  unsigned count = 0;
  const double start = now();
  if (start < 0) {
    std::puts("ERROR monotonic clock unavailable");
    return 1;
  }
  int status = 0;
  while (!interrupted && now() - start < 90) {
    kindle::Touch touch;
    const int event = input.poll(touch, 100);
    if (event < 0) {
      status = 1;
      break;
    }
    if (!event) continue;
    if (count < 500)
      std::printf("touch phase=%s raw=%d,%d normalized=%.4f,%.4f\n",
                  touch.phase == kindle::Touch::Phase::Down ? "down"
                                                            : (touch.phase == kindle::Touch::Phase::Up ? "up" : "move"),
                  touch.rawX, touch.rawY, touch.nx, touch.ny);
    ++count;
    if (touch.phase != kindle::Touch::Phase::Up) continue;
    const int x = static_cast<int>(touch.nx * (layout.width - 1)), y = static_cast<int>(touch.ny * (layout.height - 1));
    if (x >= static_cast<int>(layout.width) / 2 - 100 && x < static_cast<int>(layout.width) / 2 + 100 &&
        y >= static_cast<int>(layout.height) - 115 && y < static_cast<int>(layout.height) - 35)
      break;
    canvas.diagnostic();
    canvas.rect(x - 8, y - 8, 17, 17, 0);
    char message[96];
    std::snprintf(message, sizeof(message), "X %d Y %d", touch.rawX, touch.rawY);
    canvas.text(message, layout.height * 11 / 16);
    if (!display.present(false)) {
      status = 1;
      break;
    }
  }
  // Alarm must not interrupt the last refresh restoring the previous framebuffer.
  alarm(0);
  std::printf("diagnostic=finished touch_events=%u interrupted=%d\n", count, static_cast<int>(interrupted));
  if (!display.restore()) status = 1;
  return status;
}
}  // namespace

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IOLBF, 0);
  setvbuf(stderr, nullptr, _IOLBF, 0);
  std::puts("CrossInk PW1 hardware validation 0.1.0 (not the EPUB reader)");
  if (argc == 2 && std::strcmp(argv[1], "--version") == 0) return 0;
  if (argc == 3 && std::strcmp(argv[1], "--preview") == 0) return preview(argv[2]);
  if (argc == 3 && std::strcmp(argv[1], "--preview-landscape") == 0) return preview(argv[2], true);
  if (argc == 2 && std::strcmp(argv[1], "--probe") == 0) return run(true);
  if (argc == 2 && std::strcmp(argv[1], "--diagnostic") == 0) return run(false);
  std::fprintf(stderr, "usage: %s --version | --probe | --diagnostic | --preview file.pgm\n", argv[0]);
  return 2;
}
