#include "cinepakframe.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../base/byteorder.h"
#include "../base/memory.h"

#define MAX_STRIPS 32

struct strip {
	uint8_t v1[256][12];
	uint8_t v4[256][12];
	int32_t x1;
	int32_t y1;
	int32_t x2;
	int32_t y2;
};

struct cinepak_frame {
	int32_t width;
	int32_t height;
	uint8_t *rgb;
	struct strip strips[MAX_STRIPS];
};

static int32_t be24(const uint8_t *p)
{
	return (p[0] << 16) | (p[1] << 8) | p[2];
}

static uint8_t clip(int32_t v)
{
	return (uint8_t) (v < 0 ? 0 : v > 255 ? 255 : v);
}

static void read_codebook(uint8_t (*cb)[12], int32_t id, const uint8_t *data, const uint8_t *end)
{
	int32_t n = (id & 0x04) ? 4 : 6;
	uint32_t flag = 0;
	uint32_t mask = 0;

	for (int32_t i = 0; i < 256; ++i) {
		if ((id & 0x01) && !(mask >>= 1)) {
			if (data + 4 > end)
				return;

			flag = ReadBeU32(data);
			data += 4;
			mask = 0x80000000u;
		}

		if ((id & 0x01) && !(flag & mask))
			continue;

		if (data + n > end)
			return;

		if (n == 6) {
			int32_t u = (int8_t) data[4];
			int32_t v = (int8_t) data[5];

			for (int32_t k = 0; k < 4; ++k) {
				int32_t y = data[k];
				cb[i][3 * k + 0] = clip(y + 2 * v);
				cb[i][3 * k + 1] = clip(y - u / 2 - v);
				cb[i][3 * k + 2] = clip(y + 2 * u);
			}

		} else {
			for (int32_t k = 0; k < 4; ++k)
				cb[i][3 * k + 0] = cb[i][3 * k + 1] = cb[i][3 * k + 2] = data[k];
		}

		data += n;
	}
}

static void put_quad(struct cinepak_frame *d, int32_t x, int32_t y, const uint8_t *e, int32_t scale)
{
	for (int32_t k = 0; k < 4; ++k) {
		int32_t px = x + (k & 1) * scale;
		int32_t py = y + (k >> 1) * scale;

		for (int32_t dy = 0; dy < scale; ++dy) {
			if (py + dy >= d->height)
				break;

			for (int32_t dx = 0; dx < scale; ++dx) {
				if (px + dx >= d->width)
					break;

				uint8_t *p = d->rgb + 3 * ((py + dy) * d->width + px + dx);
				p[0] = e[3 * k + 0];
				p[1] = e[3 * k + 1];
				p[2] = e[3 * k + 2];
			}
		}
	}
}

static bool read_vectors(struct cinepak_frame *d, struct strip *s, int32_t id, const uint8_t *data,
	const uint8_t *end)
{
	uint32_t flag = 0;
	uint32_t mask = 0;

	for (int32_t y = s->y1; y < s->y2; y += 4)
		for (int32_t x = s->x1; x < s->x2; x += 4) {
			if ((id & 0x01) && !(mask >>= 1)) {
				if (data + 4 > end)
					return false;

				flag = ReadBeU32(data);
				data += 4;
				mask = 0x80000000u;
			}

			if ((id & 0x01) && !(flag & mask))
				continue;

			if (!(id & 0x02) && !(mask >>= 1)) {
				if (data + 4 > end)
					return false;

				flag = ReadBeU32(data);
				data += 4;
				mask = 0x80000000u;
			}

			if ((id & 0x02) || !(flag & mask)) {
				if (data >= end)
					return false;

				put_quad(d, x, y, s->v1[*data++], 2);

			} else {
				if (data + 4 > end)
					return false;

				put_quad(d, x, y, s->v4[data[0]], 1);
				put_quad(d, x + 2, y, s->v4[data[1]], 1);
				put_quad(d, x, y + 2, s->v4[data[2]], 1);
				put_quad(d, x + 2, y + 2, s->v4[data[3]], 1);
				data += 4;
			}
		}

	return true;
}

static bool decode_frame(struct cinepak_frame *d, const uint8_t *data, int32_t size)
{
	const uint8_t *end = data + size;
	int32_t y0 = 0;

	if (size < 10)
		return false;

	int32_t flags = data[0];
	int32_t strips = (int32_t) ReadBeU16(data + 8);
	data += 10;

	if (strips > MAX_STRIPS)
		strips = MAX_STRIPS;

	for (int32_t i = 0; i < strips; ++i) {
		struct strip *s = &d->strips[i];

		if (data + 12 > end)
			return false;

		s->y1 = (int32_t) ReadBeU16(data + 4);
		s->y2 = (int32_t) ReadBeU16(data + 8);

		if (!s->y1) {
			s->y1 = y0;
			s->y2 += y0;
		}

		s->x1 = (int32_t) ReadBeU16(data + 6);
		s->x2 = (int32_t) ReadBeU16(data + 10);
		int32_t strip_size = be24(data + 1) - 12;
		data += 12;

		if (strip_size < 0)
			return false;

		const uint8_t *strip_end = data + strip_size > end ? end : data + strip_size;

		if (i > 0 && !(flags & 0x01)) {
			memcpy(s->v1, d->strips[i - 1].v1, sizeof(s->v1));
			memcpy(s->v4, d->strips[i - 1].v4, sizeof(s->v4));
		}

		if (s->x2 > d->width || s->y2 > d->height || s->x1 >= s->x2 || s->y1 >= s->y2)
			return false;

		const uint8_t *p = data;

		while (p + 4 <= strip_end) {
			int32_t id = p[0];
			int32_t len = be24(p + 1) - 4;
			const uint8_t *chunk = p + 4;

			if (len < 0)
				return false;

			const uint8_t *chunk_end = chunk + len > strip_end ? strip_end : chunk + len;

			if (id >= 0x20 && id <= 0x27) {
				read_codebook((id & 0x02) ? s->v1 : s->v4, id, chunk, chunk_end);

			} else if (id >= 0x30 && id <= 0x32) {
				read_vectors(d, s, id, chunk, chunk_end);
				break;
			}

			p = chunk_end;
		}

		data = strip_end;
		y0 = s->y2;
	}

	return true;
}

struct cinepak_frame *CinepakFrameCreateEmpty(void)
{
	return MemAlloc(1, sizeof(struct cinepak_frame));
}

bool CinepakFrameSetSize(struct cinepak_frame *d, int32_t width, int32_t height)
{
	int32_t w = (width + 3) & ~3;
	int32_t h = (height + 3) & ~3;

	if (!d->rgb || d->width != w || d->height != h) {
		MemFree(d->rgb);
		d->rgb = MemAlloc((size_t) w * h, 3);
		d->width = w;
		d->height = h;
	}

	return true;
}

void CinepakFrameWriteOut(const struct cinepak_frame *d, int32_t out_width, int32_t out_height, int32_t bit_count,
	uint8_t *bits)
{
	int32_t bpp = bit_count / 8;
	int32_t stride = ((out_width * bit_count + 31) / 32) * 4;
	int32_t w = out_width;
	int32_t h = abs(out_height);

	for (int32_t y = 0; y < h; ++y) {
		uint8_t *row = bits + (size_t) (out_height > 0 ? h - 1 - y : y) * stride;
		const uint8_t *src = d->rgb + (size_t) y * d->width * 3;

		for (int32_t x = 0; x < w; ++x) {
			row[x * bpp + 0] = src[3 * x + 2];
			row[x * bpp + 1] = src[3 * x + 1];
			row[x * bpp + 2] = src[3 * x + 0];

			if (bpp == 4)
				row[x * bpp + 3] = 0;
		}
	}
}

struct cinepak_frame *CinepakFrameCreate(int32_t width, int32_t height)
{
	int32_t w = (width + 3) & ~3;
	int32_t h = (height + 3) & ~3;

	if (width <= 0 || height <= 0)
		return NULL;

	struct cinepak_frame *d = MemAlloc(1, sizeof(struct cinepak_frame));
	d->rgb = MemAlloc((size_t) w * h, 3);
	d->width = w;
	d->height = h;

	return d;
}

bool CinepakFrameDecode(struct cinepak_frame *f, const uint8_t *data, int32_t size)
{
	return f && data && decode_frame(f, data, size);
}

const uint8_t *CinepakFrameRgb(const struct cinepak_frame *d, int32_t *stride_pixels)
{
	if (stride_pixels)
		*stride_pixels = d ? d->width : 0;

	return d ? d->rgb : NULL;
}

void CinepakFrameDestroy(struct cinepak_frame **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct cinepak_frame *ctx = *ctx_mod;
	*ctx_mod = NULL;

	MemFree(ctx->rgb);
	MemFree(ctx);
}
