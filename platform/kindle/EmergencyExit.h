#pragma once

#include "KindleBackends.h"

namespace kindle {
// Physical portrait coordinates, before the renderer's inverse rotation.
// This detector runs on the input thread and allocates no memory.
class EmergencyExit {
 public:
  bool update(const Touch* touch, unsigned long now) {
    if (touch) {
      const bool corner = touch->nx >= 0.90f && touch->ny <= 0.10f;
      if (touch->phase == Touch::Phase::Down) {
        held_ = corner;
        since_ = now;
      } else if (touch->phase == Touch::Phase::Up || !corner) {
        held_ = false;
      }
    }
    return held_ && now - since_ >= 5000;
  }

 private:
  unsigned long since_ = 0;
  bool held_ = false;
};
}  // namespace kindle
