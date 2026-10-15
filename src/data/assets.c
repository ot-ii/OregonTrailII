#include "assets.h"

#include "../base/log.h"
#include "mecclib.h"
#include "../base/memory.h"
#include "../base/str.h"

#define MAX_SOURCES 16

struct asset_source {
	const struct asset_source_ops *ops;
	void *source;
};

struct assets {
	struct asset_source sources[MAX_SOURCES];
	int32_t count;
	int32_t primary;
};

struct assets *AssetsCreate(void)
{
	struct assets *ctx = MemAlloc(1, sizeof(struct assets));
	ctx->primary = -1;

	return ctx;
}

void AssetsDestroy(struct assets **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct assets *ctx = *ctx_mod;
	*ctx_mod = NULL;

	for (int32_t i = 0; i < ctx->count; i++)
		if (ctx->sources[i].ops->destroy)
			ctx->sources[i].ops->destroy(ctx->sources[i].source);

	MemFree(ctx);
}

void AssetsAddSource(struct assets *ctx, const struct asset_source_ops *ops, void *source, uint32_t flags)
{
	if (ctx->count >= MAX_SOURCES)
		LogFatal("too many asset sources");

	ctx->sources[ctx->count].ops = ops;
	ctx->sources[ctx->count].source = source;

	if (flags & ASSETS_PRIMARY)
		ctx->primary = ctx->count;

	ctx->count++;
}

void AssetsAddOverride(struct assets *ctx, const struct asset_source_ops *ops, void *source)
{
	if (ctx->count >= MAX_SOURCES)
		LogFatal("too many asset sources");

	for (int32_t i = ctx->count; i > 0; i--)
		ctx->sources[i] = ctx->sources[i - 1];

	ctx->sources[0].ops = ops;
	ctx->sources[0].source = source;
	ctx->count++;

	if (ctx->primary >= 0)
		ctx->primary++;
}

static struct rsrc *mecc_get_element(void *source, const char *name, int32_t id)
{
	return MeccLibGetElement(source, name, id);
}

static bool mecc_get_info(void *source, const char *name, int32_t id, char *path, size_t path_size,
	int64_t *offset, int32_t *size)
{
	if (!MeccLibFind(source, name, id, offset, size, NULL) || !*size)
		return false;

	StrCopyN(path, path_size, MeccLibPath(source));

	return true;
}

static bool mecc_is_available(void *source)
{
	return MeccLibIsAvailable(source);
}

static void mecc_destroy(void *source)
{
	struct mecclib *lib = source;
	MeccLibDestroy(&lib);
}

static const struct asset_source_ops MECC_OPS = {
	mecc_get_element,
	mecc_get_info,
	mecc_is_available,
	mecc_destroy,
};

void AssetsAddMeccLib(struct assets *ctx, struct mecclib *lib, uint32_t flags)
{
	AssetsAddSource(ctx, &MECC_OPS, lib, flags);
}

bool AssetsIsAvailable(const struct assets *ctx)
{
	if (ctx->primary < 0)
		return false;

	const struct asset_source *s = &ctx->sources[ctx->primary];

	return s->ops->is_available ? s->ops->is_available(s->source) : true;
}

int32_t AssetsSourceCount(const struct assets *ctx)
{
	return ctx->count;
}

struct mecclib *AssetsSourceMeccLib(const struct assets *ctx, int32_t index)
{
	if (index < 0 || index >= ctx->count || ctx->sources[index].ops != &MECC_OPS)
		return NULL;

	return ctx->sources[index].source;
}

struct rsrc *AssetsGetElement(struct assets *ctx, const char *name, int32_t id, bool fatal)
{
	if (!name || id < 0) {
		LogError(LOG_ERROR, "Bad parameter(s)");
		return NULL;
	}

	for (int32_t i = 0; i < ctx->count; i++) {
		struct rsrc *r = ctx->sources[i].ops->get_element(ctx->sources[i].source, name, id);

		if (r)
			return r;
	}

	if (fatal) {
		char message[100];
		StrPrintf(message, "Unable to find '%s' id# %d", name, id);
		LogError(LOG_ERROR, message);
	}

	return NULL;
}

bool AssetsGetElementInfo(struct assets *ctx, const char *name, int32_t id, char *path, size_t path_size,
	int64_t *offset, int32_t *size)
{
	if (!name || id < 0)
		return false;

	for (int32_t i = 0; i < ctx->count; i++) {
		const struct asset_source *s = &ctx->sources[i];

		if (s->ops->get_info && s->ops->get_info(s->source, name, id, path, path_size, offset, size))
			return true;
	}

	return false;
}
