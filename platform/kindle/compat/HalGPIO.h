#pragma once
#include "../KindleBackends.h"
// App-facing input facade for the first native UI milestone.
class HalGPIO {
 public:
  bool begin() { return input_.open(true); }
  int poll(kindle::Touch& touch, int timeoutMs) { return input_.poll(touch, timeoutMs); }
  bool hasTouch() const { return true; }
  bool hasEdgeSideButtons() const { return false; }
  bool isXteinkDevice() const { return false; }
  bool deviceIsX3() const { return false; }
  bool deviceIsX4() const { return false; }
  // Convert upright PW1 coordinates to GfxRenderer's canonical landscape
  // panel coordinates. GfxRenderer then handles its own logical orientation.
  static void canonicalTouch(const kindle::Touch& touch, float& nx, float& ny);

 private:
  kindle::Input input_;
};
extern HalGPIO gpio;
