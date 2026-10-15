#include "smcdecoder.h"

#include <stddef.h>
#include <string.h>

#include "../base/memory.h"

struct reader {
	const uint8_t *p;
	const uint8_t *end;
	bool bad;
};

static int32_t reader_byte(struct reader *r)
{
	if (r->p >= r->end) {
		r->bad = true;
		return 0;
	}

	return *r->p++;
}

static uint32_t reader_be16(struct reader *r)
{
	int32_t hi = reader_byte(r);

	return (uint32_t) ((hi << 8) | reader_byte(r));
}

static uint8_t *block_at(struct smc_decoder *d, int32_t n)
{
	int32_t per_row = d->stride / 4;

	return d->pixels + (size_t) (n / per_row) * 4 * d->stride + (size_t) (n % per_row) * 4;
}

static void copy_block(struct smc_decoder *d, uint8_t *to, const uint8_t *from)
{
	for (int32_t y = 0; y < 4; ++y)
		memcpy(to + y * d->stride, from + y * d->stride, 4);
}

struct smc_decoder *SmcDecoderCreate(int32_t width, int32_t height)
{
	struct smc_decoder *d = MemAlloc(1, sizeof(struct smc_decoder));

	d->width = width;
	d->height = height;
	d->stride = (width + 3) & ~3;
	d->rows = (height + 3) & ~3;
	d->pixels = MemAlloc((size_t) d->stride, (size_t) d->rows);

	return d;
}

void SmcDecoderDestroy(struct smc_decoder **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct smc_decoder *ctx = *ctx_mod;
	*ctx_mod = NULL;

	MemFree(ctx->pixels);
	MemFree(ctx);
}

bool SmcDecoderDecode(struct smc_decoder *d, const uint8_t *data, uint32_t size)
{
	struct reader r;
	int32_t total = (d->stride / 4) * (d->rows / 4);
	int32_t block = 0;
	int32_t pair_index = 0;
	int32_t quad_index = 0;
	int32_t octet_index = 0;

	if (size < 4)
		return false;

	int32_t chunk = (int32_t) (((uint32_t) data[1] << 16) | ((uint32_t) data[2] << 8) | data[3]);
	r.p = data + 4;
	r.end = data + (chunk > 4 && (uint32_t) chunk < size ? (uint32_t) chunk : size);
	r.bad = false;

	while (block < total && !r.bad && r.p < r.end) {
		int32_t op = reader_byte(&r);
		int32_t kind = op & 0xF0;
		int32_t n, i, x, y;
		int32_t count = (op & 0x10) && kind < 0x80 ? 1 + reader_byte(&r) : 1 + (op & 0x0F);

		if (count > total - block)
			count = total - block;

		switch (kind) {
			case 0x00:
			case 0x10:
				block += count;
				break;

			case 0x20:
			case 0x30:
				if (block < 1)
					return false;

				for (n = 0; n < count; ++n, ++block)
					copy_block(d, block_at(d, block), block_at(d, block - 1));

				break;

			case 0x40:
			case 0x50: {
				if (block < 2)
					return false;

				int32_t first = block - 2;
				count *= 2;

				if (count > total - block)
					count = total - block;

				for (n = 0; n < count; ++n, ++block)
					copy_block(d, block_at(d, block), block_at(d, first + (n & 1)));

				break;
			}

			case 0x60:
			case 0x70: {
				int32_t c = reader_byte(&r);

				for (n = 0; n < count; ++n, ++block) {
					uint8_t *b = block_at(d, block);

					for (y = 0; y < 4; ++y)
						memset(b + y * d->stride, c, 4);
				}

				break;
			}

			case 0x80:
			case 0x90: {
				int32_t t;

				if (kind == 0x80) {
					t = pair_index;

					for (i = 0; i < 2; ++i)
						d->pairs[t][i] = (uint8_t) reader_byte(&r);

					pair_index = (pair_index + 1) & 0xFF;

				} else {
					t = reader_byte(&r);
				}

				for (n = 0; n < count; ++n, ++block) {
					uint32_t flags = reader_be16(&r);
					uint32_t mask = 0x8000;
					uint8_t *b = block_at(d, block);

					for (y = 0; y < 4; ++y)
						for (x = 0; x < 4; ++x, mask >>= 1)
							b[y * d->stride + x] = d->pairs[t][(flags & mask) ? 1 : 0];
				}

				break;
			}

			case 0xA0:
			case 0xB0: {
				int32_t t;

				if (kind == 0xA0) {
					t = quad_index;

					for (i = 0; i < 4; ++i)
						d->quads[t][i] = (uint8_t) reader_byte(&r);

					quad_index = (quad_index + 1) & 0xFF;

				} else {
					t = reader_byte(&r);
				}

				for (n = 0; n < count; ++n, ++block) {
					uint8_t *b = block_at(d, block);

					for (y = 0; y < 4; ++y) {
						int32_t row = reader_byte(&r);

						for (x = 0; x < 4; ++x)
							b[y * d->stride + x] = d->quads[t][(row >> (6 - 2 * x)) & 3];
					}
				}

				break;
			}

			case 0xC0:
			case 0xD0: {
				int32_t t;

				if (kind == 0xC0) {
					t = octet_index;

					for (i = 0; i < 8; ++i)
						d->octets[t][i] = (uint8_t) reader_byte(&r);

					octet_index = (octet_index + 1) & 0xFF;

				} else {
					t = reader_byte(&r);
				}

				for (n = 0; n < count; ++n, ++block) {
					uint8_t *b = block_at(d, block);
					int32_t nib[12];

					for (i = 0; i < 6; ++i) {
						int32_t v = reader_byte(&r);
						nib[2 * i] = v >> 4;
						nib[2 * i + 1] = v & 0x0F;
					}

					uint32_t a = ((uint32_t) nib[0] << 20) | ((uint32_t) nib[1] << 16) | ((uint32_t) nib[2] << 12) |
						((uint32_t) nib[4] << 8) | ((uint32_t) nib[5] << 4) | (uint32_t) nib[6];
					uint32_t c = ((uint32_t) nib[8] << 20) | ((uint32_t) nib[9] << 16) | ((uint32_t) nib[10] << 12) |
						((uint32_t) nib[3] << 8) | ((uint32_t) nib[7] << 4) | (uint32_t) nib[11];

					for (y = 0; y < 4; ++y) {
						uint32_t *f = y < 2 ? &a : &c;

						for (x = 0; x < 4; ++x) {
							b[y * d->stride + x] = d->octets[t][(*f >> 21) & 7];
							*f <<= 3;
						}
					}
				}

				break;
			}

			case 0xE0:
				for (n = 0; n < count; ++n, ++block) {
					uint8_t *b = block_at(d, block);

					for (y = 0; y < 4; ++y)
						for (x = 0; x < 4; ++x)
							b[y * d->stride + x] = (uint8_t) reader_byte(&r);
				}

				break;

			default:
				return false;
		}
	}

	return !r.bad;
}
