#pragma once

#ifndef VER_MAJOR
#define VER_MAJOR 0
#endif
#ifndef VER_MINOR
#define VER_MINOR 0
#endif
#ifndef VER_BUILD
#define VER_BUILD 0
#endif

#define VER_STR_(x) #x
#define VER_STR(x) VER_STR_(x)
#define VER_STRING VER_STR(VER_MAJOR) "." VER_STR(VER_MINOR) "." VER_STR(VER_BUILD)
#define VER_STRING_RC VER_STR(VER_MAJOR.VER_MINOR.VER_BUILD)

#ifndef RC_INVOKED

#include <stdint.h>

const char *GameVersionString(void);
int32_t GameVersionMajor(void);
int32_t GameVersionMinor(void);
int32_t GameVersionBuild(void);

#endif
