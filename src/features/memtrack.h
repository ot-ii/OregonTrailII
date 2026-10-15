#pragma once

#ifdef DIAG_BUILD

#include <stddef.h>
#include <stdint.h>

int32_t MemTrackAlloc(void *caller, void *caller2, size_t size);
void MemTrackFree(int32_t site, size_t size);

void MemTrackReportNow(void);

void MemTrackLive(int32_t *blocks, int64_t *bytes);

#endif
