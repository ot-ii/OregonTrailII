// The libmatoya helpers mty_json.c and mty_hash.c call, on top of the port's own modules: memory from memory.c
// (zeroed, and an out-of-memory is fatal, as libmatoya's MTY_Alloc / MTY_Realloc), the log through Log.
// MTY_DJB2 is libmatoya's, copied from its src\crypto.c (MIT, Copyright (c) Snowcone Ltd.; libmatoya.LICENSE).

#include "mty_json.h"

#include <stdarg.h>
#include <stdio.h>

#include "../base/log.h"
#include "../base/memory.h"

void *MTY_Alloc(size_t len, size_t size)
{
	return MemAlloc(len, size);
}

void *MTY_Realloc(void *mem, size_t len, size_t size)
{
	return MemRealloc(mem, len, size);
}

void MTY_Free(void *mem)
{
	MemFree(mem);
}

char *MTY_Strdup(const char *str)
{
	return MemStrDup(str);
}

void MTY_Log(const char *fmt, ...)
{
	char text[512];
	va_list args;

	va_start(args, fmt);
	vsnprintf(text, sizeof(text), fmt, args);
	va_end(args);
	text[sizeof(text) - 1] = 0;

	Log("libmatoya: %s", text);
}

uint32_t MTY_DJB2(const char *str)
{
	uint32_t hash = 5381;

	while (*str)
		hash = ((hash << 5) + hash) + *str++;

	return hash;
}
