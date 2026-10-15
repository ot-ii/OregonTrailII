#include "cstr.h"

#include <string.h>

#include "memory.h"

struct cstr {
	char *text;
	int32_t length;
	int32_t scan_pos;
};

static const uint8_t MACROMAN_TO_ANSI[128] = {
	0xC4, 0xC5, 0xC7, 0xC9, 0xD1, 0xD6, 0xDC, 0xE1, 0xE0, 0xE2, 0xE4, 0xE3, 0xE5, 0xE7, 0xE9, 0xE8,
	0xEA, 0xEB, 0xED, 0xEC, 0xEE, 0xEF, 0xF1, 0xF3, 0xF2, 0xF4, 0xF6, 0xF5, 0xFA, 0xF9, 0xFB, 0xFC,
	0x86, 0xB0, 0xA2, 0xA3, 0xA7, 0x95, 0xB6, 0x20, 0xAE, 0xA9, 0x99, 0xB4, 0xA8, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0xA5, 0xB5, 0x20, 0x20, 0x20, 0x20, 0x20, 0xAA, 0xBA, 0x20, 0xE6, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x83, 0x20, 0x20, 0xAB, 0xBB, 0x85, 0x20, 0xC0, 0xC3, 0xD5, 0x8C, 0x9C,
	0x96, 0x97, 0x93, 0x94, 0x91, 0x92, 0xF7, 0x20, 0xFF, 0x9F, 0x20, 0xA4, 0x8B, 0x9B, 0x20, 0x20,
	0x87, 0xB9, 0xB8, 0x84, 0x89, 0xC2, 0xCA, 0xC1, 0xC8, 0xCD, 0xCE, 0xCF, 0xCC, 0xD3, 0xD4, 0x20,
	0xD2, 0xDA, 0xDB, 0xD9, 0x20, 0x88, 0x98, 0x20, 0x20, 0x20, 0x20, 0xB0, 0x20, 0x20, 0x20, 0x20
};

static int32_t text_len(const char *s)
{
	int32_t n = 0;

	if (s)
		while (s[n])
			n++;

	return n;
}

static void make_empty(struct cstr *ctx)
{
	ctx->text = MemRealloc(ctx->text, 1, 1);
	ctx->text[0] = 0;
	ctx->length = 0;
}

struct cstr *CStrCreate(void)
{
	struct cstr *ctx = MemAlloc(1, sizeof(struct cstr));
	make_empty(ctx);

	return ctx;
}

struct cstr *CStrCreateFrom(const char *s)
{
	struct cstr *ctx = CStrCreate();

	if (s && *s)
		CStrAppendBytes(ctx, s, text_len(s));

	return ctx;
}

struct cstr *CStrCreateBytes(int32_t len, const char *buf)
{
	struct cstr *ctx = CStrCreate();

	if (buf && len > 0)
		CStrAppendBytes(ctx, buf, len);

	return ctx;
}

struct cstr *CStrCreateCopy(const struct cstr *src)
{
	struct cstr *ctx = CStrCreate();

	if (src->length)
		CStrAppendBytes(ctx, src->text, src->length);

	return ctx;
}

struct cstr *CStrCreateCopyMax(const struct cstr *src, int32_t maxlen)
{
	struct cstr *ctx = CStrCreate();
	int32_t len = src->length;

	if (len) {
		if (len > maxlen)
			len = maxlen;

		if (len > 0)
			CStrAppendBytes(ctx, src->text, len);
	}

	return ctx;
}

struct cstr *CStrCreateN(const char *s, int32_t n)
{
	struct cstr *ctx = CStrCreate();

	if (s) {
		int32_t len = text_len(s);

		if (n < len)
			len = n;

		if (len > 0)
			CStrAppendBytes(ctx, s, len);
	}

	return ctx;
}

struct cstr *CStrCreateInt(int32_t value, bool grouping)
{
	char buffer[20];
	char *cursor = buffer + 19;
	int32_t digits = 0;
	int32_t group = 0;

	*cursor = 0;

	if (value) {
		bool negative = value < 0;
		uint32_t remaining = negative ? 0u - (uint32_t) value : (uint32_t) value;

		while (remaining && digits < 19) {
			*--cursor = (char) ('0' + remaining % 10);
			remaining /= 10;
			digits++;
			group++;

			if (grouping && remaining && group % 3 == 0 && digits < 19) {
				*--cursor = ',';
				digits++;
			}
		}

		if (negative && digits < 19) {
			*--cursor = '-';
			digits++;
		}

	} else {
		*--cursor = '0';
		digits++;
	}

	return CStrCreateBytes(digits, cursor);
}

void CStrDestroy(struct cstr **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct cstr *ctx = *ctx_mod;
	*ctx_mod = NULL;

	MemFree(ctx->text);
	MemFree(ctx);
}

const char *CStrGet(const struct cstr *ctx)
{
	return ctx->text;
}

char *CStrGetMutable(struct cstr *ctx)
{
	return ctx->text;
}

int32_t CStrLength(const struct cstr *ctx)
{
	return ctx->length;
}

void CStrAppendBytes(struct cstr *ctx, const char *src, int32_t n)
{
	if (n < 0)
		n = 0;

	int32_t old_len = ctx->length;
	ctx->text = MemRealloc(ctx->text, (size_t) old_len + (size_t) n + 1, 1);

	if (n)
		memmove(ctx->text + old_len, src, (size_t) n);

	ctx->text[old_len + n] = 0;
	ctx->length = old_len + n;
}

struct cstr *CStrAssign(struct cstr *ctx, const struct cstr *src)
{
	if (ctx == src)
		return ctx;

	make_empty(ctx);

	if (src->length)
		CStrAppendBytes(ctx, src->text, src->length);

	ctx->scan_pos = 0;

	return ctx;
}

struct cstr *CStrAssignBuf(struct cstr *ctx, const char *s)
{
	int32_t len = text_len(s);

	if (s >= ctx->text && s <= ctx->text + ctx->length) {
		char *copy = MemDup(s, (size_t) len + 1);
		make_empty(ctx);
		CStrAppendBytes(ctx, copy, len);
		MemFree(copy);

	} else {
		make_empty(ctx);
		CStrAppendBytes(ctx, s, len);
	}

	ctx->scan_pos = 0;

	return ctx;
}

struct cstr *CStrAppendStr(struct cstr *ctx, const struct cstr *src)
{
	if (src->length) {
		if (src == ctx) {
			char *copy = MemDup(src->text, (size_t) src->length);
			int32_t len = src->length;
			CStrAppendBytes(ctx, copy, len);
			MemFree(copy);

		} else {
			CStrAppendBytes(ctx, src->text, src->length);
		}
	}

	return ctx;
}

struct cstr *CStrAppendBuf(struct cstr *ctx, const char *s)
{
	if (s)
		CStrAppendBytes(ctx, s, text_len(s));

	return ctx;
}

void CStrAppend(struct cstr *ctx, const struct cstr *src)
{
	CStrAppendStr(ctx, src);
}

struct cstr *CStrConcat(const struct cstr *a, const struct cstr *b)
{
	struct cstr *out = CStrCreateCopy(a);
	CStrAppendBytes(out, b->text, b->length);

	return out;
}

struct cstr *CStrConcatBuf(const struct cstr *a, const char *b)
{
	struct cstr *out = CStrCreateCopy(a);
	CStrAppendBuf(out, b);

	return out;
}

struct cstr *CStrConcatBufStr(const char *a, const struct cstr *b)
{
	struct cstr *out = CStrCreateFrom(a);
	CStrAppendBytes(out, b->text, b->length);

	return out;
}

bool CStrEqual(const struct cstr *a, const struct cstr *b)
{
	return strcmp(a->text, b->text) == 0;
}

bool CStrEqualBuf(const struct cstr *a, const char *b)
{
	return strcmp(a->text, b ? b : "") == 0;
}

static int32_t compare_text(const char *a, const char *b)
{
	const uint8_t *x = (const uint8_t *) a;
	const uint8_t *y = (const uint8_t *) b;

	while (*x && *x == *y) {
		x++;
		y++;
	}

	if (*x == *y)
		return 0;

	if (!*x)
		return -1;

	if (!*y)
		return 1;

	return (int8_t) *x > (int8_t) *y ? 1 : -1;
}

bool CStrGreater(const struct cstr *a, const struct cstr *b)
{
	return compare_text(a->text, b->text) > 0;
}

bool CStrLess(const struct cstr *a, const struct cstr *b)
{
	return compare_text(a->text, b->text) < 0;
}

void CStrCopyToBuffer(const struct cstr *ctx, char *buf, int32_t size)
{
	if (!buf || size <= 0)
		return;

	int32_t n = ctx->length > 255 ? 255 : ctx->length;

	if (n > size - 1)
		n = size - 1;

	memmove(buf, ctx->text, (size_t) n);
	buf[n] = 0;
}

void CStrFromPascal(struct cstr *ctx, const char *pstr)
{
	int32_t len = (uint8_t) pstr[0];

	make_empty(ctx);

	if (len > 0)
		CStrAppendBytes(ctx, pstr + 1, len);

	ctx->scan_pos = 0;
}

char CStrCharAt(const struct cstr *ctx, int32_t i)
{
	if (i < 0 || ctx->length < i)
		return 0;

	return ctx->text[i];
}

int32_t CStrFindChar(const struct cstr *ctx, char c)
{
	for (int32_t i = 0; i < ctx->length; i++)
		if (ctx->text[i] == c)
			return i;

	return -1;
}

void CStrMacRomanToAnsi(struct cstr *ctx, int32_t mode)
{
	if (mode == 1)
		return;

	for (int32_t i = 0; i < ctx->length; i++) {
		uint8_t c = (uint8_t) ctx->text[i];

		if (c >= 0x80)
			ctx->text[i] = (char) MACROMAN_TO_ANSI[c - 0x80];
	}
}

void CStrToUpper(struct cstr *ctx)
{
	for (char *p = ctx->text; *p; p++)
		if (*p >= 'a' && *p <= 'z')
			*p -= 32;
}

void CStrToLower(struct cstr *ctx)
{
	for (char *p = ctx->text; *p; p++)
		if (*p >= 'A' && *p <= 'Z')
			*p += 32;
}

bool CStrToInt(const struct cstr *ctx, int32_t *out)
{
	int32_t remaining = ctx->length;
	int32_t place = 1;
	bool ok = true;
	bool negative = false;
	const char *cursor = ctx->text + remaining - 1;

	*out = 0;

	while (ok && remaining) {
		uint8_t c = (uint8_t) *cursor;

		if (c >= '0' && c <= '9') {
			*out = (int32_t) ((uint32_t) *out + (uint32_t) (c - '0') * (uint32_t) place);
			place = (int32_t) ((uint32_t) place * 10u);

		} else if (c == '-' && !negative) {
			negative = true;

		} else {
			ok = false;
		}

		if (ok) {
			remaining--;
			cursor--;
		}
	}

	if (ok && negative && *out > 0)
		*out = -*out;

	return ok;
}

bool CStrReplaceAll(struct cstr *ctx, const char *find, const char *repl)
{
	if (!find)
		return false;

	int32_t old_len = ctx->length;
	int32_t find_len = text_len(find);

	if (!old_len || !find_len || find_len > old_len)
		return false;

	int32_t matches = 0;

	for (const char *scan = ctx->text; *scan;) {
		if (strncmp(scan, find, (size_t) find_len) == 0) {
			matches++;
			scan += find_len;

		} else {
			scan++;
		}
	}

	if (!matches)
		return false;

	if (!repl)
		repl = "";

	int32_t repl_len = text_len(repl);
	char *snapshot = MemDup(ctx->text, (size_t) old_len + 1);
	int32_t new_len = old_len + matches * (repl_len - find_len);
	char *out = MemAlloc((size_t) new_len + 1, 1);
	char *o = out;

	for (const char *scan = snapshot; *scan;) {
		if (strncmp(scan, find, (size_t) find_len) == 0) {
			memcpy(o, repl, (size_t) repl_len);
			o += repl_len;
			scan += find_len;

		} else {
			*o++ = *scan++;
		}
	}

	*o = 0;

	MemFree(snapshot);
	MemFree(ctx->text);
	ctx->text = out;
	ctx->length = (int32_t) (o - out);
	ctx->scan_pos = 0;

	return true;
}

bool CStrReplaceObjs(struct cstr *ctx, const struct cstr *find, const struct cstr *repl)
{
	char *f = MemStrDup(find->text);
	char *r = MemStrDup(repl->text);
	bool changed = CStrReplaceAll(ctx, f, r);

	MemFree(r);
	MemFree(f);

	return changed;
}

bool CStrReplaceStr(struct cstr *ctx, const char *find, const struct cstr *repl)
{
	char *r = MemStrDup(repl->text);
	bool changed = CStrReplaceAll(ctx, find, r);

	MemFree(r);

	return changed;
}

bool CStrReplaceRsrc(struct cstr *ctx, const struct cstr *find, const char *repl)
{
	char *f = MemStrDup(find->text);
	bool changed = CStrReplaceAll(ctx, f, repl);

	MemFree(f);

	return changed;
}

static bool substr_range(const struct cstr *ctx, int32_t mode, int32_t count, int32_t *start, int32_t *len)
{
	int32_t s = -1;

	if (!mode) {
		s = 0;

	} else if (mode == -1) {
		s = ctx->length - count;

	} else if (mode > 0 && ctx->length >= mode) {
		s = mode - 1;
	}

	if (s < 0 || ctx->length <= s)
		return false;

	*start = s;
	*len = s + count > ctx->length ? ctx->length - s : count;

	return true;
}

bool CStrSubstr(struct cstr *ctx, int32_t mode, int32_t count)
{
	int32_t start, len;

	if (!substr_range(ctx, mode, count, &start, &len))
		return false;

	struct cstr *sub = CStrCreateBytes(len, ctx->text + start);
	CStrAssign(ctx, sub);
	CStrDestroy(&sub);

	return true;
}

bool CStrSubstr2(struct cstr *ctx, int32_t mode, int32_t count, struct cstr *out)
{
	int32_t start, len;

	if (!substr_range(ctx, mode, count, &start, &len))
		return false;

	struct cstr *sub = CStrCreateBytes(len, ctx->text + start);
	CStrAssign(out, sub);
	CStrDestroy(&sub);

	return true;
}

static bool find_word(struct cstr *ctx, int32_t mode, char delim, int32_t *next_pos, int32_t *word_len)
{
	if (mode > 0) {
		int32_t words = 0;
		ctx->scan_pos = 0;

		for (;;) {
			if (!CStrParseWord(ctx, -2, next_pos, word_len, delim))
				return false;

			if (++words >= mode)
				return true;

			ctx->scan_pos = *next_pos;
		}
	}

	return CStrParseWord(ctx, mode, next_pos, word_len, delim);
}

bool CStrGetWord(struct cstr *ctx, int32_t mode, char delim)
{
	if (mode == -5) {
		ctx->scan_pos = 0;
		return true;
	}

	int32_t next_pos = 0;
	int32_t word_len = 0;

	if (!find_word(ctx, mode, delim, &next_pos, &word_len))
		return false;

	struct cstr *word = CStrCreateBytes(word_len, ctx->text + ctx->scan_pos);
	CStrAssign(ctx, word);
	CStrDestroy(&word);

	ctx->scan_pos = next_pos;

	return true;
}

bool CStrGetWord2(struct cstr *ctx, int32_t mode, struct cstr *out, char delim)
{
	if (mode == -5) {
		ctx->scan_pos = 0;
		return true;
	}

	int32_t next_pos = 0;
	int32_t word_len = 0;

	if (!find_word(ctx, mode, delim, &next_pos, &word_len))
		return false;

	struct cstr *word = CStrCreateBytes(word_len, ctx->text + ctx->scan_pos);
	CStrAssign(out, word);
	CStrDestroy(&word);

	ctx->scan_pos = next_pos;

	return true;
}

static bool is_space(uint8_t c, char delim)
{
	return c == ' ' || (!(uint8_t) delim && c == '\t');
}

bool CStrParseWord(struct cstr *ctx, int32_t mode, int32_t *pos, int32_t *len, char delim)
{
	bool ok = false;
	bool in_quote = false;
	const uint8_t *cursor = (const uint8_t *) ctx->text;

	*len = 0;
	*pos = ctx->scan_pos;

	switch (mode) {
		case -4:
		case -2:
		case 0:
			if (!mode)
				ctx->scan_pos = 0;

			if (ctx->scan_pos < 0 || ctx->scan_pos > ctx->length)
				break;

			cursor += ctx->scan_pos;

			while (is_space(*cursor, delim)) {
				cursor++;
				ctx->scan_pos++;
			}

			if (!*cursor)
				break;

			ok = true;

			if (*cursor < 0x20) {
				*len = 1;

			} else {
				if (*cursor == '"') {
					in_quote = true;
					cursor++;
					ctx->scan_pos++;
				}

				while (*cursor && (in_quote ? *cursor != '"' : *cursor > 0x20)) {
					(*len)++;
					cursor++;
				}
			}

			if (mode == -2 || mode == 0) {
				*pos = ctx->scan_pos + *len;

				if (in_quote)
					(*pos)++;

			} else {
				*pos = ctx->scan_pos;
			}
			break;

		case -1:
			ctx->scan_pos = ctx->length - 1;
			// FALL THROUGH INTENTIONAL
		case -3:
			if (ctx->scan_pos < 0 || ctx->scan_pos > ctx->length)
				break;

			cursor += ctx->scan_pos;

			while (is_space(*cursor, delim) && ctx->scan_pos > 0) {
				cursor--;
				ctx->scan_pos--;
			}

			if (!ctx->scan_pos)
				break;

			ok = true;

			if (*cursor < 0x20) {
				*len = 1;

			} else {
				if (*cursor == '"') {
					in_quote = true;
					cursor--;
					ctx->scan_pos--;
				}

				while (ctx->scan_pos && (in_quote ? *cursor != '"' : *cursor > 0x20)) {
					(*len)++;
					cursor--;
					ctx->scan_pos--;
				}
			}

			*pos = ctx->scan_pos;
			break;
	}

	return ok;
}
