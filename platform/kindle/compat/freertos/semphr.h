#pragma once
#include "FreeRTOS.h"
struct KindleSemaphore;
using SemaphoreHandle_t = KindleSemaphore*;
SemaphoreHandle_t xSemaphoreCreateMutex();
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex();
BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t);
BaseType_t xSemaphoreGive(SemaphoreHandle_t);
TaskHandle_t xSemaphoreGetMutexHolder(SemaphoreHandle_t);
void vSemaphoreDelete(SemaphoreHandle_t);
inline BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t m, TickType_t t) { return xSemaphoreTake(m, t); }
inline BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t m) { return xSemaphoreGive(m); }
