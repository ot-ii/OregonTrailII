#include "rsrc.h"

#include <string.h>

#include "assets.h"
#include "gfxdev.h"
#include "../base/list.h"
#include "../base/log.h"
#include "../base/memory.h"
#include "../base/str.h"

struct rsrc {
	char name[8];
	int32_t id;
	int32_t storage;
	uint8_t *data;
	int32_t size;
};

static struct obarray *g_rsrc_cache;
static int32_t g_unpack_truncations;

struct rsrc *RsrcCreate(const char *name, int32_t id, int32_t size, int32_t storage)
{
	struct rsrc *ctx = MemAlloc(1, sizeof(struct rsrc));

	if (name) {
		StrCopy(ctx->name, name);
		ctx->id = id;
		ctx->storage = storage;
		RsrcResize(ctx, size);
	}

	return ctx;
}

void RsrcDestroy(struct rsrc **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct rsrc *ctx = *ctx_mod;
	*ctx_mod = NULL;

	MemFree(ctx->data);
	MemFree(ctx);
}

uint8_t *RsrcData(struct rsrc *ctx)
{
	return ctx->data;
}

int32_t RsrcSize(const struct rsrc *ctx)
{
	return ctx->size;
}

void RsrcResize(struct rsrc *ctx, int32_t size)
{
	if (size < 0)
		size = 0;

	ctx->data = MemRealloc(ctx->data, (size_t) size + 1, 1);

	if (size > ctx->size)
		memset(ctx->data + ctx->size, 0, (size_t) (size - ctx->size) + 1);

	ctx->size = size;
}

const char *RsrcName(const struct rsrc *ctx)
{
	return ctx->name;
}

int32_t RsrcId(const struct rsrc *ctx)
{
	return ctx->id;
}

int32_t RsrcStorage(const struct rsrc *ctx)
{
	return ctx->storage;
}

static void free_cached(void *item)
{
	struct rsrc *r = item;
	RsrcDestroy(&r);
}

void RsrcCacheClear(void)
{
	ObarrayDestroy(&g_rsrc_cache);
}

int32_t RsrcUnpackTruncations(void)
{
	return g_unpack_truncations;
}


struct unpack {
	uint8_t *base;
	uint8_t *end;
	const uint8_t *fmt;
	int32_t pos;
	uint8_t *dst;
};

static void swap16(uint8_t *p)
{
	uint8_t t = p[0];
	p[0] = p[1];
	p[1] = t;
}

static void swap32(uint8_t *p)
{
	uint8_t t = p[0];
	p[0] = p[3];
	p[3] = t;
	t = p[1];
	p[1] = p[2];
	p[2] = t;
}

static int32_t bounded_strlen(const uint8_t *p, const uint8_t *end)
{
	int32_t n = 0;

	while (p + n < end && p[n])
		n++;

	return n;
}

static int32_t swap_field(struct unpack *u, int32_t field)
{
	int32_t width = 0;
	uint8_t *p = u->dst;

	if (field == 2 || field == 5 || field == 11 || field == 18) {
		width = 2;

	} else if (field == 3 || field == 6 || field == 10) {
		width = 4;

	} else if (field == 25) {
		width = 8;
	}

	if (width && p + width > u->end) {
		u->dst = u->end;
		g_unpack_truncations++;
		return 0;
	}

	switch (field) {
		case 1:
		case 4:
		case 17:
			return 1;

		case 2:
		case 5:
			swap16(p);
			return 2;

		case 3:
		case 6:
			swap32(p);
			return 4;

		case 9:
			return (int8_t) p[0] + 1;

		case 10:
			swap32(p);
			return (int32_t) (p[0] | p[1] << 8 | p[2] << 16 | (uint32_t) p[3] << 24) + 4;

		case 11:
		case 18:
			swap16(p);
			return (int16_t) (p[0] | p[1] << 8) + 2;

		case 12: {
			int32_t size = (int8_t) p[0] + 1;
			return size % 2 ? size : size + 1;
		}

		case 13: {
			int32_t size = (int8_t) p[0] + 1;
			return size % 2 ? size + 1 : size;
		}

		case 14:
			return bounded_strlen(p, u->end) + 1;

		case 15: {
			int32_t size = bounded_strlen(p, u->end) + 1;
			return size % 2 ? size + 1 : size;
		}

		case 16: {
			int32_t size = bounded_strlen(p, u->end) + 1;
			return size % 2 ? size : size + 1;
		}

		case 25:
			for (int32_t i = 0; i < 4; i++) {
				uint8_t t = p[i];
				p[i] = p[7 - i];
				p[7 - i] = t;
			}
			return 8;
	}

	return 0;
}

static int32_t swap_interp(struct unpack *u)
{
	int32_t err = 0;
	int32_t op = (int8_t) u->fmt[u->pos];
	u->pos++;

	switch (op) {
		case 19:
		case 22:
			while (u->fmt[u->pos] != 20 && u->fmt[u->pos] && !err)
				err = swap_interp(u);

			if (op == 19)
				u->dst++;

			if (u->fmt[u->pos])
				u->pos++;
			break;

		case 24: {
			int32_t loop_pos = u->pos;

			while (u->dst < u->end && !err) {
				u->pos = loop_pos;

				while (u->fmt[u->pos] != 20 && u->fmt[u->pos] && !err)
					err = swap_interp(u);
			}

			err = 1;
			break;
		}

		case 21:
		case 23: {
			if (u->dst + 2 > u->end) {
				u->dst = u->end;
				g_unpack_truncations++;
				return 1;
			}

			swap16(u->dst);
			int32_t count = (int16_t) (u->dst[0] | u->dst[1] << 8);
			int32_t body = u->pos;
			u->dst += 2;

			for (int32_t i = op == 23 ? 1 : 0; i <= count && !err; i++) {
				u->pos = body;
				err = swap_interp(u);
			}
			break;
		}

		default: {
			int32_t size = swap_field(u, op);

			if (!size)
				err = u->pos - 1;

			u->dst += size;
			break;
		}
	}

	return err;
}

int32_t RsrcUnpack(struct rsrc *ctx, const uint8_t *recipe)
{
	struct unpack u;
	u.base = ctx->data;
	u.end = ctx->data + ctx->size;
	u.fmt = recipe;
	u.pos = 0;
	u.dst = ctx->data;

	int32_t err = 0;

	while (recipe[u.pos] && !err)
		err = swap_interp(&u);

	return err;
}

int32_t RsrcLoadAndUnpack(struct rsrc *ctx)
{
	if (ctx->storage != RSRC_PACKED)
		return 0;

	char recipe_name[16];
	StrPrintf(recipe_name, "T%s", ctx->name);

	struct rsrc *recipe = NULL;

	for (int32_t i = 0; g_rsrc_cache && i < ObarrayCount(g_rsrc_cache); i++) {
		struct rsrc *r = ObarrayGetAt(g_rsrc_cache, i);

		if (strcmp(r->name, recipe_name) == 0) {
			recipe = r;
			break;
		}
	}

	if (!recipe) {
		recipe = AssetsGetElement(g_data_lib, recipe_name, 0, true);

		if (!recipe)
			return 1;

		if (recipe->storage == RSRC_TEXT) {
			char message[100];
			StrPrintf(message, "'%s' resource %d not packed", recipe_name, ctx->id);
			LogError(LOG_ERROR, message);
		}

		if (!g_rsrc_cache)
			g_rsrc_cache = ObarrayCreate(free_cached);

		ObarrayAppend(g_rsrc_cache, recipe);
	}

	return RsrcUnpack(ctx, recipe->data);
}
