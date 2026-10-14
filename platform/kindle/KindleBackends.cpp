#include "KindleBackends.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace kindle {
namespace {
// PW1 (Pearl) ABI: the later Carta/Zelda ioctl layouts are incompatible.
// Verified against KOReader's ffi/mxcfb_kindle_h.lua and framebuffer_mxcfb.lua.
struct Rect {
  uint32_t top, left, width, height;
};
struct AltBuffer {
  uint32_t address, width, height;
  Rect region;
};
struct Update {
  Rect region;
  uint32_t waveform, mode, marker, historyBw, historyGray;
  int32_t temperature;
  uint32_t flags;
  AltBuffer alternate;
};
static_assert(sizeof(Update) == 72, "PW1 ioctl layout");
constexpr unsigned long sendUpdate = _IOW('F', 0x2e, Update);
constexpr unsigned long waitUpdate = _IOW('F', 0x2f, uint32_t);
// Absent from the PW1's old kernel headers; reserved evdev value on newer hosts.
constexpr uint16_t synDropped = 3;
static_assert(sendUpdate == 0x4048462e, "PW1 send ioctl");
static_assert(waitUpdate == 0x4004462f, "PW1 Pearl wait ioctl");

void error(const char* operation) { std::fprintf(stderr, "ERROR %s: %s\n", operation, std::strerror(errno)); }
}  // namespace

size_t Layout::row(uint32_t y) const { return (static_cast<size_t>(yoffset) + y) * stride + xoffset; }
bool Layout::valid() const {
  if (width == 0 || height == 0 || width > 2048 || height > 2048 || memory > 16 * 1024 * 1024) return false;
  if (static_cast<uint64_t>(xoffset) + width > stride) return false;
  const uint64_t end = (static_cast<uint64_t>(yoffset) + height - 1) * stride + xoffset + width;
  return end <= memory;
}

float TouchDecoder::normalize(int value, int minimum, int maximum) {
  if (maximum <= minimum) return 0;
  const float n = static_cast<float>(static_cast<int64_t>(value) - minimum) /
                  static_cast<float>(static_cast<int64_t>(maximum) - minimum);
  return n < 0 ? 0 : (n > 1 ? 1 : n);
}
void TouchDecoder::configure(int xcode, int ycode, int xmin, int xmax, int ymin, int ymax, bool contactKey) {
  *this = TouchDecoder{};
  xcode_ = xcode;
  ycode_ = ycode;
  xmin_ = xmin;
  xmax_ = xmax;
  ymin_ = ymin;
  ymax_ = ymax;
  contactKey_ = contactKey;
}
bool TouchDecoder::consume(const input_event& event, Touch& touch) {
  if (event.type == EV_ABS) {
    if (event.code == xcode_) {
      x_ = event.value;
      haveX_ = true;
      changed_ = true;
      mtFrame_ = true;
    }
    if (event.code == ycode_) {
      y_ = event.value;
      haveY_ = true;
      changed_ = true;
      mtFrame_ = true;
    }
    if (!contactKey_ && event.code == ABS_MT_TRACKING_ID) {
      trackingSeen_ = true;
      trackingActive_ = event.value >= 0;
    }
  } else if (event.type == EV_KEY && event.code == BTN_TOUCH) {
    down_ = event.value != 0;
  }
  if (event.type == EV_SYN && event.code == SYN_MT_REPORT) typeA_ = true;
  if (event.type != EV_SYN || event.code != SYN_REPORT) return false;
  // Legacy type-A drivers can express release as an empty MT frame.
  if (!contactKey_)
    down_ = typeA_ ? mtFrame_ && (!trackingSeen_ || trackingActive_) : (trackingSeen_ ? trackingActive_ : mtFrame_);
  const bool emit = haveX_ && haveY_ && (down_ != previousDown_ || (down_ && changed_));
  if (emit) {
    touch.phase = !down_ ? Touch::Phase::Up : (!previousDown_ ? Touch::Phase::Down : Touch::Phase::Move);
    touch.rawX = x_;
    touch.rawY = y_;
    touch.nx = normalize(x_, xmin_, xmax_);
    touch.ny = normalize(y_, ymin_, ymax_);
  }
  previousDown_ = down_;
  changed_ = false;
  mtFrame_ = false;
  return emit;
}

Input::~Input() {
  if (grabbed_) ioctl(fd_, EVIOCGRAB, 0);
  if (fd_ >= 0) close(fd_);
}
bool Input::open(bool exclusive) {
  fd_ = ::open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd_ < 0) {
    error("open touchscreen");
    return false;
  }
  char name[128] = {};
  if (ioctl(fd_, EVIOCGNAME(sizeof(name)), name) < 0 || std::strcmp(name, "cyttsp") != 0) {
    std::fprintf(stderr, "ERROR expected PW1 cyttsp touchscreen; got '%s'\n", name);
    return false;
  }
  int xcode = ABS_MT_POSITION_X, ycode = ABS_MT_POSITION_Y;
  input_absinfo x{}, y{};
  if (ioctl(fd_, EVIOCGABS(xcode), &x) < 0 || ioctl(fd_, EVIOCGABS(ycode), &y) < 0 || x.maximum <= x.minimum ||
      y.maximum <= y.minimum) {
    xcode = ABS_X;
    ycode = ABS_Y;
    if (ioctl(fd_, EVIOCGABS(xcode), &x) < 0 || ioctl(fd_, EVIOCGABS(ycode), &y) < 0) {
      error("touch axes");
      return false;
    }
  }
  if (x.maximum <= x.minimum || y.maximum <= y.minimum) {
    std::fprintf(stderr, "ERROR invalid touch axis ranges\n");
    return false;
  }
  // Byte bitset, rather than native unsigned long, has the same layout on host/ARM.
  uint8_t keys[(KEY_MAX + 8) / 8] = {};
  if (ioctl(fd_, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) < 0) {
    error("touch key capabilities");
    return false;
  }
  const bool contactKey = (keys[BTN_TOUCH / 8] & (1U << (BTN_TOUCH % 8))) != 0;
  decoder_.configure(xcode, ycode, x.minimum, x.maximum, y.minimum, y.maximum, contactKey);
  std::printf("touch device=%s xcode=%d range=%d:%d ycode=%d range=%d:%d contact_key=%d\n", name, xcode, x.minimum,
              x.maximum, ycode, y.minimum, y.maximum, contactKey);
  if (exclusive) {
    if (ioctl(fd_, EVIOCGRAB, 1) < 0) {
      error("exclusive touchscreen grab");
      return false;
    }
    grabbed_ = true;
  }
  return true;
}
int Input::poll(Touch& touch, int timeoutMs) {
  pollfd pfd{fd_, POLLIN, 0};
  const int result = ::poll(&pfd, 1, timeoutMs);
  if (result < 0) {
    if (errno == EINTR) return 0;
    error("poll touch");
    return -1;
  }
  if (result == 0) return 0;
  if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
    std::fprintf(stderr, "ERROR touch disconnected\n");
    return -1;
  }
  // Read one event at a time: never discard the remainder of a read batch.
  input_event event{};
  const ssize_t count = read(fd_, &event, sizeof(event));
  if (count < 0 && (errno == EAGAIN || errno == EINTR)) return 0;
  if (count != static_cast<ssize_t>(sizeof(event))) {
    std::fprintf(stderr, "ERROR short input event\n");
    return -1;
  }
  if (event.type == EV_SYN && event.code == synDropped) {
    std::fprintf(stderr, "ERROR dropped touch events; ending diagnostic\n");
    return -1;
  }
  return decoder_.consume(event, touch) ? 1 : 0;
}

Display::~Display() {
  if (modified_) restore();
  if (mapping_) munmap(mapping_, layout_.memory);
  std::free(pixels_);
  std::free(original_);
  if (fd_ >= 0) close(fd_);
}
bool Display::open(bool writable) {
  fd_ = ::open("/dev/fb0", (writable ? O_RDWR : O_RDONLY) | O_CLOEXEC);
  if (fd_ < 0) {
    error("open framebuffer");
    return false;
  }
  fb_fix_screeninfo fixed{};
  fb_var_screeninfo variable{};
  if (ioctl(fd_, FBIOGET_FSCREENINFO, &fixed) < 0 || ioctl(fd_, FBIOGET_VSCREENINFO, &variable) < 0) {
    error("framebuffer geometry");
    return false;
  }
  std::printf("display id=%.16s size=%ux%u bpp=%u stride=%u offsets=%u,%u rotate=%u memory=%u\n", fixed.id,
              variable.xres, variable.yres, variable.bits_per_pixel, fixed.line_length, variable.xoffset,
              variable.yoffset, variable.rotate, fixed.smem_len);
  layout_ = {variable.xres, variable.yres, fixed.line_length, variable.xoffset, variable.yoffset, fixed.smem_len};
  if (std::strncmp(fixed.id, "mxc_epdc_fb", sizeof(fixed.id)) != 0 || variable.bits_per_pixel != 8 ||
      variable.rotate > 3 || !layout_.valid() ||
      !((layout_.width == 758 && layout_.height == 1024) || (layout_.width == 1024 && layout_.height == 758))) {
    std::fprintf(stderr, "ERROR unsupported PW1 framebuffer layout; no pixels written\n");
    return false;
  }
  if (!writable) return true;
  void* mapping = mmap(nullptr, layout_.memory, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
  if (mapping == MAP_FAILED) {
    error("mmap framebuffer");
    return false;
  }
  mapping_ = static_cast<uint8_t*>(mapping);
  const size_t count = static_cast<size_t>(layout_.width) * layout_.height;
  // Reused canvas + exit snapshot; at PW1 dimensions each is < 0.8 MB.
  pixels_ = static_cast<uint8_t*>(std::malloc(count));
  original_ = static_cast<uint8_t*>(std::malloc(count));
  if (!pixels_ || !original_) {
    std::fprintf(stderr, "ERROR framebuffer allocation\n");
    return false;
  }
  for (uint32_t y = 0; y < layout_.height; ++y)
    std::memcpy(original_ + static_cast<size_t>(y) * layout_.width, mapping_ + layout_.row(y), layout_.width);
  return true;
}
bool Display::update(bool full) {
  Update update{};
  update.region = {0, 0, layout_.width, layout_.height};
  update.waveform = 2;  // GC16, including partial updates in this first validation build.
  update.mode = full ? 1 : 0;
  update.marker = ++marker_;
  update.historyBw = 1;
  update.historyGray = 2;
  update.temperature = 0x1001;  // TEMP_USE_PAPYRUS on PW1.
  if (ioctl(fd_, sendUpdate, &update) < 0) {
    error("MXCFB_SEND_UPDATE");
    return false;
  }
  uint32_t marker = update.marker;
  if (ioctl(fd_, waitUpdate, &marker) < 0) {
    error("MXCFB_WAIT_FOR_UPDATE_COMPLETE_PEARL");
    return false;
  }
  std::printf("refresh marker=%u mode=%s result=ok\n", update.marker, full ? "full" : "partial");
  return true;
}
bool Display::present(bool full) {
  if (!mapping_ || !pixels_ || !original_) return false;
  modified_ = true;
  for (uint32_t y = 0; y < layout_.height; ++y)
    std::memcpy(mapping_ + layout_.row(y), pixels_ + static_cast<size_t>(y) * layout_.width, layout_.width);
  return update(full);
}
bool Display::restore() {
  if (!modified_) return true;
  for (uint32_t y = 0; y < layout_.height; ++y)
    std::memcpy(mapping_ + layout_.row(y), original_ + static_cast<size_t>(y) * layout_.width, layout_.width);
  modified_ = false;  // Do not retry an unsupported ioctl indefinitely during destruction.
  return update(true);
}
}  // namespace kindle
