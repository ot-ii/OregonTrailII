#include "image.h"

#include <string.h>

#include "../base/byteorder.h"
#include "decode.h"
#include "../base/memory.h"
#include "rsrc.h"

#define DECODE_PADDING 16

struct image *ImageCreate(int32_t width, int32_t height, int32_t format, int32_t scale)
{
	struct image *ctx = MemAlloc(1, sizeof(struct image));
	ctx->width = width > 0 ? width : 0;
	ctx->height = height > 0 ? height : 0;
	ctx->format = format;
	ctx->scale = scale > 0 ? scale : 1;
	ctx->stride = format == IMAGE_BGRA32 ? ctx->width * 4 : ctx->width;
	ctx->pixels = MemAlloc((size_t) ctx->stride * (size_t) ctx->height + 1, 1);

	return ctx;
}

void ImageDestroy(struct image **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct image *ctx = *ctx_mod;
	*ctx_mod = NULL;

	MemFree(ctx->pixels);
	MemFree(ctx);
}

int32_t ImageLogicalWidth(const struct image *ctx)
{
	return ctx->width / ctx->scale;
}

int32_t ImageLogicalHeight(const struct image *ctx)
{
	return ctx->height / ctx->scale;
}

struct cbmp {
	uint8_t *data;
	int32_t size;
};

struct cbmp *CbmpCreateFromData(const uint8_t *data, int32_t size)
{
	struct cbmp *ctx = MemAlloc(1, sizeof(struct cbmp));
	ctx->size = size > 0 ? size : 0;
	ctx->data = MemAlloc((size_t) ctx->size + DECODE_PADDING, 1);

	if (data && ctx->size)
		memcpy(ctx->data, data, (size_t) ctx->size);

	return ctx;
}

struct cbmp *CbmpCreate(const struct rsrc *r)
{
	return CbmpCreateFromData(RsrcData((struct rsrc *) r), RsrcSize(r));
}

void CbmpDestroy(struct cbmp **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct cbmp *ctx = *ctx_mod;
	*ctx_mod = NULL;

	MemFree(ctx->data);
	MemFree(ctx);
}

int32_t CbmpFrameCount(const struct cbmp *ctx)
{
	return ctx->size >= 2 ? ReadLe16(ctx->data) : 0;
}

static const uint8_t *frame_entry(const struct cbmp *ctx, int32_t frame)
{
	if (frame < 0 || frame >= CbmpFrameCount(ctx) || 2 + 12 * frame + 8 > ctx->size)
		return NULL;

	return ctx->data + 2 + 12 * frame;
}

int32_t CbmpFrameSize(const struct cbmp *ctx, int32_t frame, int32_t *w, int32_t *h)
{
	const uint8_t *e = frame_entry(ctx, frame);

	*w = 0;
	*h = 0;

	if (!e)
		return 2;

	*w = ReadLe16(e);
	*h = ReadLe16(e + 2);

	return 0;
}

static const uint8_t *frame_data(const struct cbmp *ctx, const uint8_t *e)
{
	int32_t offset = ReadLe32(e + 4);

	if (offset < 0 || offset >= ctx->size)
		return NULL;

	return ctx->data + offset;
}

int32_t CbmpDecode(const struct cbmp *ctx, int32_t frame, uint8_t *dst, int32_t stride, int32_t mode, bool bottom_up)
{
	const uint8_t *e = frame_entry(ctx, frame);

	if (!e)
		return 2;

	const uint8_t *data = frame_data(ctx, e);

	if (!data || !dst)
		return 0;

	int32_t w = ReadLe16(e);
	int32_t h = ReadLe16(e + 2);

	DecodeFrame(data, ctx->size - (int32_t) (data - ctx->data), w, bottom_up ? -h : h, dst, stride, mode);

	return 0;
}

int32_t CbmpDecodeSkip(const struct cbmp *ctx, int32_t frame, uint8_t *dst, int32_t stride, uint8_t *drawn)
{
	const uint8_t *e = frame_entry(ctx, frame);

	if (!e)
		return 2;

	const uint8_t *data = frame_data(ctx, e);

	if (!data || !dst)
		return 0;

	DecodeFrameSkip(data, ctx->size - (int32_t) (data - ctx->data), ReadLe16(e), ReadLe16(e + 2), dst, stride, drawn);

	return 0;
}

struct image *CbmpToImage(const struct cbmp *ctx, int32_t frame)
{
	int32_t w, h;

	if (CbmpFrameSize(ctx, frame, &w, &h) != 0 || w <= 0 || h <= 0)
		return NULL;

	int32_t tile_w = (w + 7) & ~7;
	int32_t tile_h = (h + 7) & ~7;
	uint8_t *buf = MemAlloc((size_t) tile_w * (size_t) tile_h, 1);

	CbmpDecode(ctx, frame, buf, tile_w, DECODE_OPAQUE, false);

	struct image *img = ImageCreate(w, h, IMAGE_INDEXED8, 1);

	for (int32_t y = 0; y < h; y++)
		memcpy(img->pixels + (size_t) y * img->stride, buf + (size_t) y * tile_w, (size_t) w);

	MemFree(buf);

	return img;
}
