#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void StrCopyN(char *dst, size_t size, const char *src);
void StrCatN(char *dst, size_t size, const char *src);
int32_t StrPrintfN(char *dst, size_t size, const char *fmt, ...);

#define StrCopy(dst, src)          StrCopyN((dst), sizeof(dst), (src))
#define StrCat(dst, src)           StrCatN((dst), sizeof(dst), (src))
#define StrPrintf(dst, fmt, ...)   StrPrintfN((dst), sizeof(dst), (fmt), __VA_ARGS__)

int32_t StrLen(const char *str);
bool StrEqualNoCase(const char *a, const char *b);
