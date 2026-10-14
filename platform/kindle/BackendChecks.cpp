#include <climits>
#include <cmath>
#include <cstdio>

#include "EmergencyExit.h"
#include "KindleBackends.h"

namespace {
unsigned failures = 0;
void check(bool passed, const char* label) {
  if (!passed) {
    ++failures;
    std::fprintf(stderr, "FAIL %s\n", label);
  }
}
bool feed(kindle::TouchDecoder& decoder, kindle::Touch& touch, int type, int code, int value = 0) {
  input_event event{};
  event.type = type;
  event.code = code;
  event.value = value;
  return decoder.consume(event, touch);
}
void contactChecks() {
  kindle::TouchDecoder decoder;
  kindle::Touch touch;
  decoder.configure(ABS_X, ABS_Y, 10, 1010, 20, 2020, true);
  feed(decoder, touch, EV_ABS, ABS_X, 510);
  feed(decoder, touch, EV_ABS, ABS_Y, 1020);
  feed(decoder, touch, EV_KEY, BTN_TOUCH, 1);
  check(feed(decoder, touch, EV_SYN, SYN_REPORT) && touch.phase == kindle::Touch::Phase::Down, "legacy touch down");
  check(std::fabs(touch.nx - 0.5f) < 0.0001f && std::fabs(touch.ny - 0.5f) < 0.0001f, "axis minima and normalization");
  check(!feed(decoder, touch, EV_SYN, SYN_REPORT), "no duplicate tap from unchanged frame");
  feed(decoder, touch, EV_ABS, ABS_X, 1010);
  check(feed(decoder, touch, EV_SYN, SYN_REPORT) && touch.phase == kindle::Touch::Phase::Move && touch.nx == 1,
        "one-axis move retains other axis");
  feed(decoder, touch, EV_KEY, BTN_TOUCH, 0);
  check(feed(decoder, touch, EV_SYN, SYN_REPORT) && touch.phase == kindle::Touch::Phase::Up, "legacy touch up");
  check(!feed(decoder, touch, EV_SYN, SYN_REPORT), "no duplicate release");
  decoder.configure(ABS_MT_POSITION_X, ABS_MT_POSITION_Y, 0, 100, 0, 200, false);
  feed(decoder, touch, EV_ABS, ABS_MT_TRACKING_ID, 3);
  feed(decoder, touch, EV_ABS, ABS_MT_POSITION_X, 30);
  feed(decoder, touch, EV_ABS, ABS_MT_POSITION_Y, 80);
  feed(decoder, touch, EV_SYN, SYN_MT_REPORT);
  check(feed(decoder, touch, EV_SYN, SYN_REPORT) && touch.phase == kindle::Touch::Phase::Down, "type A touch down");
  feed(decoder, touch, EV_SYN, SYN_MT_REPORT);
  check(feed(decoder, touch, EV_SYN, SYN_REPORT) && touch.phase == kindle::Touch::Phase::Up,
        "type A empty frame releases");
  decoder.configure(ABS_MT_POSITION_X, ABS_MT_POSITION_Y, 0, 100, 0, 200, false);
  feed(decoder, touch, EV_ABS, ABS_MT_TRACKING_ID, 7);
  feed(decoder, touch, EV_ABS, ABS_MT_POSITION_X, 30);
  feed(decoder, touch, EV_ABS, ABS_MT_POSITION_Y, 80);
  check(feed(decoder, touch, EV_SYN, SYN_REPORT) && touch.phase == kindle::Touch::Phase::Down, "type B touch down");
  check(!feed(decoder, touch, EV_SYN, SYN_REPORT), "type B stationary contact stays down");
  feed(decoder, touch, EV_ABS, ABS_MT_TRACKING_ID, -1);
  check(feed(decoder, touch, EV_SYN, SYN_REPORT) && touch.phase == kindle::Touch::Phase::Up, "type B tracking release");
}
}  // namespace

int main() {
  kindle::Layout layout{758, 1024, 768, 0, 0, 768 * 6144};
  check(layout.valid(), "PW1 padded stride");
  check(layout.row(1023) == 768 * 1023, "last visible row");
  layout.yoffset = 1024;
  check(layout.valid(), "pan offset within virtual framebuffer");
  layout.memory = 768 * 2047;
  check(!layout.valid(), "reject truncated mapped memory");
  layout.memory = 768 * 6144;
  layout.xoffset = 11;
  check(!layout.valid(), "reject row crossing stride");
  layout.xoffset = 0;
  layout.height = 0;
  check(!layout.valid(), "reject zero height before row subtraction");
  layout.height = 1024;
  layout.yoffset = UINT_MAX;
  check(!layout.valid(), "reject overflowed offset");
  check(kindle::TouchDecoder::normalize(INT_MAX, INT_MIN, INT_MAX) == 1, "avoid signed range overflow");
  check(kindle::TouchDecoder::normalize(-1, 0, 100) == 0, "clamp below minimum");
  check(kindle::TouchDecoder::normalize(101, 0, 100) == 1, "clamp above maximum");
  kindle::EmergencyExit emergency;
  kindle::Touch point;
  point.phase = kindle::Touch::Phase::Down;
  point.nx = 0.95f;
  point.ny = 0.05f;
  check(!emergency.update(&point, 100), "emergency hold starts without exit");
  check(!emergency.update(nullptr, 5099), "short emergency hold does not exit");
  check(emergency.update(nullptr, 5100), "stationary emergency hold exits without new input");
  point.phase = kindle::Touch::Phase::Up;
  check(!emergency.update(&point, 5101), "release clears emergency hold");
  point.phase = kindle::Touch::Phase::Down;
  emergency.update(&point, 6000);
  point.phase = kindle::Touch::Phase::Move;
  point.nx = 0.8f;
  check(!emergency.update(&point, 7000) && !emergency.update(nullptr, 12000), "leaving corner cancels hold");
  point.phase = kindle::Touch::Phase::Down;
  point.nx = 0.95f;
  emergency.update(&point, ULONG_MAX - 2000);
  check(emergency.update(nullptr, 2999), "emergency timer survives monotonic counter wrap");
  contactChecks();
  std::printf("backend_checks failures=%u\n", failures);
  return failures ? 1 : 0;
}
