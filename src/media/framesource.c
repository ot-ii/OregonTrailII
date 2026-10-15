#include "framesource.h"

#include <stddef.h>
#include <string.h>

#include "cinepakframe.h"
#include "../data/image.h"
#include "../base/memory.h"
#include "movfile.h"
#include "smcdecoder.h"

struct frame_source {
	struct mov_file mov;
	struct smc_decoder *smc;
	struct cinepak_frame *cvid;
	struct image indexed;
	struct image *bgra;
	bool bgra_stale;
};

struct frame_source *FrameSourceOpen(const char *path)
{
	struct frame_source *ctx = MemAlloc(1, sizeof(struct frame_source));

	if (!MovFileOpen(path, &ctx->mov)) {
		MemFree(ctx);
		return NULL;
	}

	int32_t w = ctx->mov.video.width;
	int32_t h = ctx->mov.video.height;

	if (ctx->mov.video.fourcc == MOV_FCC_SMC) {
		ctx->smc = SmcDecoderCreate(w, h);

	} else {
		ctx->cvid = CinepakFrameCreate(w, h);
	}

	if (!ctx->smc && !ctx->cvid) {
		FrameSourceDestroy(&ctx);
		return NULL;
	}

	if (ctx->smc) {
		ctx->indexed.width = w;
		ctx->indexed.height = h;
		ctx->indexed.stride = ctx->smc->stride;
		ctx->indexed.format = IMAGE_INDEXED8;
		ctx->indexed.scale = 1;
		ctx->indexed.pixels = ctx->smc->pixels;

	} else {
		ctx->bgra = ImageCreate(w, h, IMAGE_BGRA32, 1);
		ctx->bgra_stale = true;
	}

	return ctx;
}

void FrameSourceDestroy(struct frame_source **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct frame_source *ctx = *ctx_mod;
	*ctx_mod = NULL;

	SmcDecoderDestroy(&ctx->smc);
	CinepakFrameDestroy(&ctx->cvid);
	MovFileClose(&ctx->mov);
	ImageDestroy(&ctx->bgra);
	MemFree(ctx);
}

int32_t FrameSourceWidth(const struct frame_source *ctx)
{
	return ctx->mov.video.width;
}

int32_t FrameSourceHeight(const struct frame_source *ctx)
{
	return ctx->mov.video.height;
}

int32_t FrameSourceFrameCount(const struct frame_source *ctx)
{
	return ctx->mov.video.samples;
}

int32_t FrameSourceDurationMs(const struct frame_source *ctx)
{
	return ctx->mov.duration_ms;
}

int32_t FrameSourceFrameAt(const struct frame_source *ctx, int32_t ms)
{
	return MovFileFrameAt(&ctx->mov, ms);
}

int32_t FrameSourceFrameTime(const struct frame_source *ctx, int32_t index)
{
	const struct mov_track *t = &ctx->mov.video;

	if (index < 0)
		index = 0;

	if (index >= t->samples)
		index = t->samples - 1;

	return t->delay_ms + (int32_t) ((double) (t->time[index] - t->media_start) * 1000.0 / t->timescale);
}

int32_t FrameSourceDecode(struct frame_source *ctx, int32_t index)
{
	const uint8_t *data;
	uint32_t size;
	bool ok = false;

	if (!MovFileReadFrame(&ctx->mov, index, &data, &size))
		return FRAME_NOT_READ;

	if (ctx->smc) {
		ok = SmcDecoderDecode(ctx->smc, data, size);

	} else if (ctx->cvid) {
		ok = CinepakFrameDecode(ctx->cvid, data, (int32_t) size);
		ctx->bgra_stale = true;
	}

	return ok ? FRAME_DECODED : FRAME_DAMAGED;
}

void FrameSourceRewind(struct frame_source *ctx)
{
	if (ctx->smc)
		memset(ctx->smc->pixels, 0, (size_t) ctx->smc->stride * ctx->smc->rows);
}

const struct image *FrameSourceImage(struct frame_source *ctx)
{
	if (ctx->smc)
		return &ctx->indexed;

	if (ctx->bgra_stale) {
		int32_t src_stride;
		const uint8_t *rgb = CinepakFrameRgb(ctx->cvid, &src_stride);
		struct image *img = ctx->bgra;

		for (int32_t y = 0; y < img->height; ++y) {
			uint8_t *row = img->pixels + (size_t) y * img->stride;
			const uint8_t *src = rgb + (size_t) y * src_stride * 3;

			for (int32_t x = 0; x < img->width; ++x) {
				row[4 * x + 0] = src[3 * x + 2];
				row[4 * x + 1] = src[3 * x + 1];
				row[4 * x + 2] = src[3 * x + 0];
				row[4 * x + 3] = 0xFF;
			}
		}

		ctx->bgra_stale = false;
	}

	return ctx->bgra;
}

const uint8_t (*FrameSourcePalette(const struct frame_source *ctx))[3]
{
	return ctx->smc ? (const uint8_t (*)[3]) ctx->mov.video.palette : NULL;
}

bool FrameSourceSound(struct frame_source *ctx, uint8_t **pcm, int32_t *bytes, int32_t *rate, int32_t *channels,
	int32_t *bits)
{
	return MovFileReadSound(&ctx->mov, pcm, bytes, rate, channels, bits);
}
