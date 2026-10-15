#pragma once

#include <stddef.h>

void *MemAlloc(size_t count, size_t size);
void *MemRealloc(void *mem, size_t count, size_t size);
void *MemDup(const void *mem, size_t size);
char *MemStrDup(const char *str);
void MemFree(void *mem);
