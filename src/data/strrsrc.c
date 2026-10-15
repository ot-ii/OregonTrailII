#include "strrsrc.h"

#include <string.h>

#include "assets.h"
#include "../base/byteorder.h"
#include "../base/cstr.h"
#include "../base/memory.h"
#include "rsrc.h"
#include "../base/str.h"

struct strrsrc {
	int32_t id;
	int32_t count;
	uint8_t *data;
	int32_t size;
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

struct strrsrc *StrRsrcCreate(void)
{
	struct strrsrc *ctx = MemAlloc(1, sizeof(struct strrsrc));
	ctx->id = -1;

	return ctx;
}

struct strrsrc *StrRsrcLoad(struct assets *lib, int32_t id, bool fatal)
{
	if (!lib)
		return NULL;

	struct rsrc *r = AssetsGetElement(lib, "STRLST", id, fatal);

	if (!r || RsrcSize(r) == 0 || (RsrcStorage(r) != RSRC_PACKED && RsrcStorage(r) != RSRC_NATIVE)) {
		RsrcDestroy(&r);
		return NULL;
	}

	RsrcLoadAndUnpack(r);

	struct strrsrc *ctx = StrRsrcCreate();
	ctx->id = id;
	ctx->size = RsrcSize(r);
	ctx->data = MemDup(RsrcData(r), (size_t) ctx->size + 1);
	ctx->count = ctx->size >= 2 ? ReadLe16(ctx->data) : 0;

	RsrcDestroy(&r);

	return ctx;
}

struct strrsrc *StrRsrcCreateCopy(const struct strrsrc *src)
{
	struct strrsrc *ctx = StrRsrcCreate();
	ctx->id = src->id;
	ctx->count = src->count;
	ctx->size = src->size;
	ctx->data = src->data ? MemDup(src->data, (size_t) src->size + 1) : NULL;

	return ctx;
}

void StrRsrcDestroy(struct strrsrc **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct strrsrc *ctx = *ctx_mod;
	*ctx_mod = NULL;

	MemFree(ctx->data);
	MemFree(ctx);
}

int32_t StrRsrcCount(const struct strrsrc *ctx)
{
	return ctx->count;
}

int32_t StrRsrcId(const struct strrsrc *ctx)
{
	return ctx->id;
}

static const char *str_at(const struct strrsrc *ctx, int32_t index)
{
	if (ctx->id == -1 || index < 0 || index >= ctx->count || 2 + 4 * index + 4 > ctx->size)
		return NULL;

	int32_t offset = ReadLe32(ctx->data + 2 + 4 * index);

	if (offset < 0 || offset >= ctx->size)
		return NULL;

	return (const char *) ctx->data + offset;
}

const char *StrRsrcGetAt(const struct strrsrc *ctx, int32_t index)
{
	return str_at(ctx, index);
}

struct cstr *StrRsrcGetCStr(const struct strrsrc *ctx, int32_t index)
{
	const char *s = str_at(ctx, index);

	return s ? CStrCreateFrom(s) : NULL;
}

void StrRsrcCopyTo(const struct strrsrc *ctx, int32_t index, char *buf, int32_t max)
{
	const char *s = str_at(ctx, index);

	if (s && buf && max > 1) {
		StrCopyN(buf, (size_t) max, s);

	} else if (buf) {
		*buf = 0;
	}
}

int32_t StrRsrcStrlenAt(const struct strrsrc *ctx, int32_t index)
{
	const char *s = str_at(ctx, index);

	return s ? (int32_t) strlen(s) : -1;
}

int32_t StrRsrcIndexOf(const struct strrsrc *ctx, const char *s)
{
	if (!s)
		return -1;

	for (int32_t i = 0; i < ctx->count; i++) {
		const char *t = str_at(ctx, i);

		if (t && strcmp(t, s) == 0)
			return i;
	}

	return -1;
}

void StrRsrcAppend(struct strrsrc *ctx, const char *s)
{
	if (!s)
		s = "";

	int32_t len = (int32_t) strlen(s);

	if (ctx->count == 0) {
		int32_t size = 6 + len + 1;
		MemFree(ctx->data);
		ctx->data = MemAlloc((size_t) size + 1, 1);
		WriteLe16(ctx->data, 1);
		WriteLe32(ctx->data + 2, 6);
		memcpy(ctx->data + 6, s, (size_t) len + 1);
		ctx->size = size;
		ctx->count = 1;
		ctx->id = 0;
		return;
	}

	int32_t old_table_end = 2 + 4 * ctx->count;
	int32_t size = ctx->size + 4 + len + 1;
	uint8_t *data = MemAlloc((size_t) size + 1, 1);

	WriteLe16(data, ctx->count + 1);

	for (int32_t i = 0; i < ctx->count; i++)
		WriteLe32(data + 2 + 4 * i, (uint32_t) (ReadLe32(ctx->data + 2 + 4 * i) + 4));

	memcpy(data + old_table_end + 4, ctx->data + old_table_end, (size_t) (ctx->size - old_table_end));
	WriteLe32(data + old_table_end, (uint32_t) (size - len - 1));
	memcpy(data + size - len - 1, s, (size_t) len + 1);

	MemFree(ctx->data);
	ctx->data = data;
	ctx->size = size;
	ctx->count++;
}

void StrRsrcAppendAll(struct strrsrc *ctx, const struct strrsrc *other)
{
	for (int32_t i = 0; i < other->count; i++)
		StrRsrcAppend(ctx, str_at(other, i));
}

void StrRsrcRemapHighChars(struct strrsrc *ctx, int32_t mode)
{
	if (mode == 1)
		return;

	for (int32_t i = 0; i < ctx->count; i++) {
		uint8_t *t = (uint8_t *) str_at(ctx, i);

		for (; t && *t; t++)
			if (*t >= 0x80)
				*t = MACROMAN_TO_ANSI[*t - 0x80];
	}
}
