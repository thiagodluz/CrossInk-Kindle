#pragma once

#include <cstddef>
#include <cstdint>
#include <linux/fb.h>
#include <linux/input.h>

namespace kindle {

// The diagnostic executable owns these Linux backends. The application HAL
// integration is a subsequent milestone, after their PW1 behavior is validated.
struct Layout {
  uint32_t width = 0, height = 0, stride = 0, xoffset = 0, yoffset = 0;
  size_t memory = 0;
  bool valid() const;
  size_t row(uint32_t y) const;
};

struct Touch {
  enum class Phase { Down, Move, Up };
  Phase phase = Phase::Move;
  int rawX = 0, rawY = 0;
  float nx = 0, ny = 0;
};

class TouchDecoder {
 public:
  void configure(int xcode, int ycode, int xmin, int xmax, int ymin, int ymax, bool contactKey);
  bool consume(const input_event& event, Touch& touch);
  static float normalize(int value, int minimum, int maximum);

 private:
  int xcode_ = ABS_X, ycode_ = ABS_Y, xmin_ = 0, xmax_ = 1, ymin_ = 0, ymax_ = 1;
  int x_ = 0, y_ = 0;
  bool contactKey_ = false, down_ = false, previousDown_ = false;
  bool haveX_ = false, haveY_ = false, changed_ = false, mtFrame_ = false;
  bool typeA_ = false, trackingSeen_ = false, trackingActive_ = false;
};

class Input {
 public:
  ~Input();
  bool open(bool exclusive);
  // 1: touch, 0: timeout/irrelevant event, -1: device error.
  int poll(Touch& touch, int timeoutMs);
 private:
  int fd_ = -1;
  bool grabbed_ = false;
  TouchDecoder decoder_;
};

class Display {
 public:
  ~Display();
  bool open(bool writable);
  bool present(bool full);
  bool restore();
  const Layout& layout() const { return layout_; }
  uint8_t* pixels() { return pixels_; }
 private:
  bool update(bool full);
  int fd_ = -1;
  Layout layout_;
  uint8_t *mapping_ = nullptr, *pixels_ = nullptr, *original_ = nullptr;
  uint32_t marker_ = 0;
  bool modified_ = false;
};

}  // namespace kindle
