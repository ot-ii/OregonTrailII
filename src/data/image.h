#pragma once

#include <stdbool.h>
#include <stdint.h>

enum image_format {
	IMAGE_INDEXED8 = 1,
	IMAGE_BGRA32 = 2,
};

struct image {
	int32_t width;
	int32_t height;
	int32_t stride;
	int32_t format;
	int32_t scale;
	uint8_t *pixels;
};

struct image *ImageCreate(int32_t width, int32_t height, int32_t format, int32_t scale);
void ImageDestroy(struct image **ctx_mod);
int32_t ImageLogicalWidth(const struct image *ctx);
int32_t ImageLogicalHeight(const struct image *ctx);

struct rsrc;
struct cbmp;

struct cbmp *CbmpCreate(const struct rsrc *r);
struct cbmp *CbmpCreateFromData(const uint8_t *data, int32_t size);
void CbmpDestroy(struct cbmp **ctx_mod);

int32_t CbmpFrameCount(const struct cbmp *ctx);
int32_t CbmpFrameSize(const struct cbmp *ctx, int32_t frame, int32_t *w, int32_t *h);

int32_t CbmpDecode(const struct cbmp *ctx, int32_t frame, uint8_t *dst, int32_t stride, int32_t mode, bool bottom_up);
int32_t CbmpDecodeSkip(const struct cbmp *ctx, int32_t frame, uint8_t *dst, int32_t stride, uint8_t *drawn);

struct image *CbmpToImage(const struct cbmp *ctx, int32_t frame);
