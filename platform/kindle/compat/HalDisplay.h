#pragma once
#include <GrayscaleCapabilities.h>

#include <cstdint>

#include "../KindleBackends.h"
class HalDisplay {
 public:
  using GrayscaleMode = freeink::GrayscaleMode;
  using GrayscaleCapabilities = freeink::GrayscaleCapabilities;
  enum RefreshMode { FULL_REFRESH, HALF_REFRESH, FAST_REFRESH };
  // Canonical GfxRenderer panel, not the /dev/fb0 memory layout.
  static constexpr uint16_t DISPLAY_WIDTH = 1024, DISPLAY_HEIGHT = 758, DISPLAY_WIDTH_BYTES = 128;
  static constexpr uint32_t BUFFER_SIZE = DISPLAY_WIDTH_BYTES * DISPLAY_HEIGHT;
  ~HalDisplay();
  bool initialize(bool preview = false);
  void begin(bool = false) { initialize(); }
  bool ready() const { return frame_ != nullptr; }
  bool healthy() const { return healthy_; }
  bool restore() { return preview_ || backend_.restore(); }
  void handoffToSystem() { backend_.handoffToSystem(); }
  bool savePreview(const char* path) const;
  uint8_t* getFrameBuffer() const { return frame_; }
  uint16_t getDisplayWidth() const { return DISPLAY_WIDTH; }
  uint16_t getDisplayHeight() const { return DISPLAY_HEIGHT; }
  uint16_t getDisplayWidthBytes() const { return DISPLAY_WIDTH_BYTES; }
  uint32_t getBufferSize() const { return BUFFER_SIZE; }
  void clearScreen(uint8_t color = 255) const;
  void drawImage(const uint8_t*, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool = false) const;
  void drawImageTransparent(const uint8_t*, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool = false) const;
  void displayBuffer(RefreshMode = FAST_REFRESH, bool = false);
  void displayBufferAsync(RefreshMode mode = FAST_REFRESH) { displayBuffer(mode); }
  void waitRefreshComplete() {}
  bool supportsAsyncRefresh() const { return false; }
  bool supportsAsyncGrayscaleBase() const { return false; }
  GrayscaleCapabilities grayscaleCapabilities(GrayscaleMode = GrayscaleMode::Overlay) const { return {}; }
  bool supportsStripGrayscale() const { return false; }
  bool shouldSkipImageBlanking() const { return false; }
  void preconditionGrayscale() {}
  void preconditionGrayscale(uint16_t, uint16_t, uint16_t, uint16_t) {}
  void displayGrayscaleBase(RefreshMode fallback = HALF_REFRESH, bool = false) { displayBuffer(fallback); }
  bool displayGrayscaleBase(GrayscaleMode, RefreshMode fallback, bool = false) {
    displayBuffer(fallback);
    return false;
  }
  void copyGrayscaleBuffers(const uint8_t*, const uint8_t*) {}
  void copyGrayscaleLsbBuffers(const uint8_t*) {}
  void copyGrayscaleMsbBuffers(const uint8_t*) {}
  void cleanupGrayscaleBuffers(const uint8_t*) {}
  void displayGrayBuffer(bool = false) { displayBuffer(); }
  void writeGrayscalePlaneStrip(bool, const uint8_t*, uint16_t, uint16_t) {}
  uint8_t* lendFrameBufferStorage(uint32_t* count);
  void returnFrameBufferStorage();
  void setInverted(bool value) { inverted_ = value; }
  bool isInverted() const { return inverted_; }

 private:
  uint8_t shadeAt(int x, int y) const;
  kindle::Display backend_;
  uint8_t* frame_ = nullptr;
  bool preview_ = false, healthy_ = true, lent_ = false, inverted_ = false, first_ = true;
};
extern HalDisplay display;
