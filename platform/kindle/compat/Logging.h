#pragma once
void logPrintf(const char* level, const char* origin, const char* format, ...);
#define LOG_ERR(origin, format, ...) logPrintf("ERR", origin, format "\n", ##__VA_ARGS__)
#define LOG_INF(origin, format, ...) logPrintf("INF", origin, format "\n", ##__VA_ARGS__)
#define LOG_DBG(origin, format, ...) logPrintf("DBG", origin, format "\n", ##__VA_ARGS__)
