#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef OTII_TEST
#include "../../tests/testframework.h"
#endif

static LogOutputFunc g_log_output;
static void *g_log_output_opaque;
static LogFatalFunc g_log_fatal;
static void *g_log_fatal_opaque;

void LogSetOutput(LogOutputFunc func, void *opaque)
{
	g_log_output = func;
	g_log_output_opaque = opaque;
}

void LogSetFatal(LogFatalFunc func, void *opaque)
{
	g_log_fatal = func;
	g_log_fatal_opaque = opaque;
}

static void log_v(const char *fmt, va_list args, char *text, size_t size)
{
	vsnprintf(text, size, fmt, args);
	text[size - 1] = 0;
}

void Log(const char *fmt, ...)
{
	char text[1024];
	va_list args;

	va_start(args, fmt);
	log_v(fmt, args, text, sizeof(text));
	va_end(args);

	if (g_log_output)
		g_log_output(text, g_log_output_opaque);
}

static void fatal_exit(const char *text)
{
	if (g_log_output)
		g_log_output(text, g_log_output_opaque);

	if (g_log_fatal)
		g_log_fatal(text, g_log_fatal_opaque);

	exit(1);
}

void LogFatal(const char *fmt, ...)
{
	char text[1024];
	va_list args;

	va_start(args, fmt);
	log_v(fmt, args, text, sizeof(text));
	va_end(args);

	fatal_exit(text);
}

void LogErrorAt(int32_t level, const char *func, const char *error_string, const char *filename, int32_t line)
{
	if (level != LOG_ERROR && level != LOG_ASSERT)
		return;

#ifdef OTII_TEST
	TFatal(func, error_string, filename, line);
#endif

	char text[512];
	snprintf(text, sizeof(text), "FATAL ERROR: %s\r\r%s\rFile: %s  Line: %d", func ? func : "",
		error_string ? error_string : "", filename ? filename : "", line);

	fatal_exit(text);
}
