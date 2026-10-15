#pragma once

#ifdef DIAG_BUILD

#include <stddef.h>
#include <stdio.h>

void *FreeGuardAlloc(size_t count, size_t size);
void *FreeGuardRealloc(void *mem, size_t size);
void FreeGuardFree(void *mem);

void FreeGuardDescribe(FILE *f, const void *addr, const char *name);

#endif
