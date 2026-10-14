#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "compat/HalDisplay.h"
#include "compat/HalGPIO.h"

HalDisplay display;
HalGPIO gpio;
HalDisplay::~HalDisplay() { std::free(frame_); }
bool HalDisplay::initialize(bool preview) {
  if (frame_) return true;
  preview_ = preview;
  if (!preview_ && !backend_.open(true)) return false;
  // First UI milestone only accepts the hardware-validated portrait mode.
  if (!preview_ && (backend_.layout().width != 758 || backend_.layout().height != 1024)) return false;
  frame_ = static_cast<uint8_t*>(std::malloc(BUFFER_SIZE));
  if (!frame_) return false;
  clearScreen();
  return true;
}
void HalDisplay::clearScreen(uint8_t color) const {
  if (frame_ && !lent_) std::memset(frame_, color, BUFFER_SIZE);
}
void HalDisplay::drawImage(const uint8_t* image, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool) const {
  if (!frame_ || lent_ || !image) return;
  const size_t stride = (static_cast<size_t>(w) + 7) / 8;
  for (uint32_t dy = 0; dy < h && y + dy < DISPLAY_HEIGHT; ++dy)
    for (uint32_t dx = 0; dx < w && x + dx < DISPLAY_WIDTH; ++dx) {
      const bool white = (image[dy * stride + dx / 8] & (0x80U >> (dx % 8))) != 0;
      auto& byte = frame_[(y + dy) * DISPLAY_WIDTH_BYTES + (x + dx) / 8];
      const uint8_t mask = 0x80U >> ((x + dx) % 8);
      if (white)
        byte |= mask;
      else
        byte &= static_cast<uint8_t>(~mask);
    }
}
void HalDisplay::drawImageTransparent(const uint8_t* image, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                      bool) const {
  if (!frame_ || lent_ || !image) return;
  const size_t stride = (static_cast<size_t>(w) + 7) / 8;
  for (uint32_t dy = 0; dy < h && y + dy < DISPLAY_HEIGHT; ++dy)
    for (uint32_t dx = 0; dx < w && x + dx < DISPLAY_WIDTH; ++dx)
      if ((image[dy * stride + dx / 8] & (0x80U >> (dx % 8))) == 0)
        frame_[(y + dy) * DISPLAY_WIDTH_BYTES + (x + dx) / 8] &= static_cast<uint8_t>(~(0x80U >> ((x + dx) % 8)));
}
uint8_t HalDisplay::shadeAt(int x, int y) const {
  const int px = y, py = DISPLAY_HEIGHT - 1 - x;
  const bool white = (frame_[py * DISPLAY_WIDTH_BYTES + px / 8] & (0x80U >> (px % 8))) != 0;
  return (white != inverted_) ? 255 : 0;
}
void HalDisplay::displayBuffer(RefreshMode mode, bool) {
  if (!frame_ || lent_) {
    healthy_ = false;
    return;
  }
  if (preview_) return;
  auto* pixels = backend_.pixels();
  for (int y = 0; y < 1024; ++y)
    for (int x = 0; x < 758; ++x) pixels[y * 758 + x] = shadeAt(x, y);
  healthy_ = backend_.present(first_ || mode != FAST_REFRESH);
  first_ = false;
}
bool HalDisplay::savePreview(const char* path) const {
  FILE* out = std::fopen(path, "wb");
  if (!out) return false;
  bool ok = std::fprintf(out, "P5\n758 1024\n255\n") > 0;
  uint8_t row[758];  // One output scanline; avoids allocating a second full preview framebuffer.
  for (int y = 0; y < 1024 && ok; ++y) {
    for (int x = 0; x < 758; ++x) row[x] = shadeAt(x, y);
    ok = std::fwrite(row, 1, sizeof(row), out) == sizeof(row);
  }
  return std::fclose(out) == 0 && ok;
}
uint8_t* HalDisplay::lendFrameBufferStorage(uint32_t* count) {
  if (lent_ || !frame_) return nullptr;
  lent_ = true;
  if (count) *count = BUFFER_SIZE;
  return frame_;
}
void HalDisplay::returnFrameBufferStorage() {
  lent_ = false;
  clearScreen();
}
void HalGPIO::canonicalTouch(const kindle::Touch& touch, float& nx, float& ny) {
  const int lx = static_cast<int>(std::lround(touch.nx * 757));
  const int ly = static_cast<int>(std::lround(touch.ny * 1023));
  nx = (ly + 0.5f) / 1024;
  ny = (757 - lx + 0.5f) / 758;
}
