#pragma once
#include <pthread.h>

#include <cstdint>
using BaseType_t = int;
using UBaseType_t = unsigned;
using TickType_t = uint32_t;
using TaskHandle_t = void*;
constexpr BaseType_t pdTRUE = 1, pdFALSE = 0, pdPASS = 1;
constexpr TickType_t portMAX_DELAY = UINT32_MAX;
#define pdMS_TO_TICKS(ms) (ms)
struct portMUX_TYPE {
  pthread_mutex_t mutex;
};
#define portMUX_INITIALIZER_UNLOCKED {PTHREAD_MUTEX_INITIALIZER}
inline void portENTER_CRITICAL(portMUX_TYPE* m) { pthread_mutex_lock(&m->mutex); }
inline void portEXIT_CRITICAL(portMUX_TYPE* m) { pthread_mutex_unlock(&m->mutex); }
