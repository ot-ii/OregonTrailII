#include "memory.h"

#include <stdlib.h>
#include <string.h>

#include "log.h"

#ifdef DIAG_BUILD
#include "../features/freeguard.h"
#endif

void *MemAlloc(size_t count, size_t size)
{
#ifdef DIAG_BUILD
	void *mem = FreeGuardAlloc(count ? count : 1, size ? size : 1);
#else
	void *mem = calloc(count ? count : 1, size ? size : 1);
#endif

	if (!mem)
		LogFatal("calloc(%zu, %zu) failed", count, size);

	return mem;
}

void *MemRealloc(void *mem, size_t count, size_t size)
{
	size_t bytes = count * size;

	if (size && bytes / size != count)
		LogFatal("realloc size overflow (%zu x %zu)", count, size);

#ifdef DIAG_BUILD
	void *out = FreeGuardRealloc(mem, bytes ? bytes : 1);
#else
	void *out = realloc(mem, bytes ? bytes : 1);
#endif

	if (!out)
		LogFatal("realloc(%zu) failed", bytes);

	return out;
}

void *MemDup(const void *mem, size_t size)
{
	void *out = MemAlloc(1, size);

	if (mem && size)
		memcpy(out, mem, size);

	return out;
}

char *MemStrDup(const char *str)
{
	if (!str)
		str = "";

	return MemDup(str, strlen(str) + 1);
}

void MemFree(void *mem)
{
#ifdef DIAG_BUILD
	FreeGuardFree(mem);
#else
	free(mem);
#endif
}
