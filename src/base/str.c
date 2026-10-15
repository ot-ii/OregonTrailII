#include "str.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void StrCopyN(char *dst, size_t size, const char *src)
{
	if (!dst || !size)
		return;

	if (!src)
		src = "";

	size_t len = strnlen(src, size - 1);
	memmove(dst, src, len);
	dst[len] = 0;
}

void StrCatN(char *dst, size_t size, const char *src)
{
	if (!dst || !size)
		return;

	size_t used = strnlen(dst, size);

	if (used >= size) {
		dst[size - 1] = 0;
		return;
	}

	StrCopyN(dst + used, size - used, src);
}

int32_t StrPrintfN(char *dst, size_t size, const char *fmt, ...)
{
	if (!dst || !size)
		return 0;

	va_list args;
	va_start(args, fmt);
	int32_t n = vsnprintf(dst, size, fmt, args);
	va_end(args);

	dst[size - 1] = 0;

	return n;
}

int32_t StrLen(const char *str)
{
	return str ? (int32_t) strlen(str) : 0;
}

bool StrEqualNoCase(const char *a, const char *b)
{
	if (!a || !b)
		return a == b;

	for (; *a && *b; a++, b++) {
		uint8_t ca = (uint8_t) *a;
		uint8_t cb = (uint8_t) *b;

		if (ca >= 'A' && ca <= 'Z')
			ca += 32;

		if (cb >= 'A' && cb <= 'Z')
			cb += 32;

		if (ca != cb)
			return false;
	}

	return *a == *b;
}
