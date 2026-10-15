#pragma once

#ifdef DIAG_BUILD

#include <stddef.h>

void CrashLogInstall(void);

void CrashLogAllocFailure(size_t bytes);

#endif
