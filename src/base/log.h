#pragma once

#include <stdint.h>

#define LOG_ERROR   0
#define LOG_ASSERT  1
#define LOG_WARNING 2
#define LOG_INFO    3

typedef void (*LogOutputFunc)(const char *text, void *opaque);
typedef void (*LogFatalFunc)(const char *text, void *opaque);

void LogSetOutput(LogOutputFunc func, void *opaque);
void LogSetFatal(LogFatalFunc func, void *opaque);

void Log(const char *fmt, ...);
void LogFatal(const char *fmt, ...);
void LogErrorAt(int32_t level, const char *func, const char *error_string, const char *filename, int32_t line);

#define LogError(level, error_string) LogErrorAt((level), __func__, (error_string), __FILE__, __LINE__)
