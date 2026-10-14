#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "WString.h"
class Print {
 public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t* p, size_t count) {
    size_t done = 0;
    while (done < count && write(p[done]) == 1) ++done;
    return done;
  }
  virtual void flush() {}
  size_t print(const char* text) { return text ? write(reinterpret_cast<const uint8_t*>(text), std::strlen(text)) : 0; }
  size_t print(const String& text) { return print(text.c_str()); }
  size_t println(const char* text = "") { return print(text) + print("\n"); }
  size_t println(int value) {
    char text[32];
    std::snprintf(text, sizeof(text), "%d", value);
    return println(text);
  }
};
