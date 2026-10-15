#include "decode.h"

#include <string.h>

#include "../base/byteorder.h"

struct sink {
	int32_t mode;
	uint8_t *dst;
	uint8_t *cur;
	int32_t stride;
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
	bool bottom_up;
	bool allow_skip;
	uint8_t *drawn;
	int32_t tiles_down;
};

#define SINK_SKIP 2

struct bits {
	const uint8_t *src;
	int32_t len;
};

static int32_t g_out_of_range_reads;

static uint8_t bits_byte(const struct bits *b, int32_t offset)
{
	if (offset >= 0 && offset < b->len)
		return b->src[offset];

	g_out_of_range_reads++;
	return 0;
}

int32_t DecodeOutOfRangeReads(void)
{
	return g_out_of_range_reads;
}

static uint32_t window32(const struct bits *b, int32_t bit_pos)
{
	int32_t at = bit_pos >> 3;
	uint32_t w = (uint32_t) bits_byte(b, at) << 24 | (uint32_t) bits_byte(b, at + 1) << 16 |
		(uint32_t) bits_byte(b, at + 2) << 8 | bits_byte(b, at + 3);

	return w << (bit_pos & 7);
}

static uint16_t window16(const struct bits *b, int32_t bit_pos)
{
	int32_t at = bit_pos >> 3;

	return (uint16_t) ((uint16_t) (bits_byte(b, at) << 8 | bits_byte(b, at + 1)) << (bit_pos & 7));
}

static void write_block(struct sink *s, const uint8_t *src)
{
	int32_t rows = s->height - s->y;

	if (rows > 8)
		rows = 8;

	if (s->mode == SINK_SKIP) {
		if (!(ReadLeU32(src) == 0 && s->allow_skip)) {
			s->drawn[(s->x / 8) * s->tiles_down + s->y / 8] = 1;

			for (int32_t r = 0; r < rows; r++)
				memcpy(s->cur + r * s->stride, src + 8 * r, 8);
		}

		s->y += 8;

		if (s->y >= s->height) {
			s->y = 0;
			s->x += 8;
		}

		s->cur = s->dst + s->x + s->y * s->stride;
		return;
	}

	int32_t step = s->bottom_up ? -s->stride : s->stride;
	uint8_t *cur = s->cur;

	for (int32_t r = 0; r < rows; r++, src += 8, cur += step) {
		if (s->mode == DECODE_TRANSPARENT) {
			for (int32_t i = 0; i < 8; i++)
				if (src[i])
					cur[i] = src[i];

		} else {
			memcpy(cur, src, 8);
		}
	}

	s->y += 8;

	if (s->y >= s->height) {
		s->y = 0;
		s->x += 8;
		s->cur = s->dst + s->x;

	} else {
		s->cur = cur;
	}
}

static uint16_t varlen_window(uint8_t hi, uint8_t lo)
{
	return (uint16_t) ((uint16_t) hi << 8 | lo);
}

static int32_t varlen_bit(uint16_t window, uint8_t count)
{
	return (window >> (count & 15)) & 1;
}

static uint16_t varlen_sar(uint16_t window, uint8_t count)
{
	uint32_t amount = count & 31;
	int16_t signed_window = (int16_t) window;

	if (amount >= 16)
		return (uint16_t) (signed_window < 0 ? 0xFFFF : 0);

	return (uint16_t) (signed_window >> amount);
}

static int32_t unpack_bits_n(const struct bits *src, int32_t bit_pos, uint8_t width, int32_t count, uint8_t *out)
{
	while (count-- > 0) {
		*out++ = (uint8_t) (window16(src, bit_pos) >> (16 - width));
		bit_pos += width;
	}

	return bit_pos;
}

static int32_t decode_varlen_bits(const struct bits *src, int32_t bit_pos, uint8_t n, uint8_t c, int32_t max_sym,
	int32_t count, uint8_t *out)
{
	if ((1 << n) > max_sym)
		return unpack_bits_n(src, bit_pos, n, count, out);

	if (count <= 0)
		return bit_pos;

	uint8_t k = (uint8_t) (n - 1);
	uint8_t m = (uint8_t) (c + k - 1);
	int16_t short_range = (int16_t) (1 << k);
	uint8_t short_mask = (uint8_t) (short_range - 1);
	uint8_t m_plus_1 = (uint8_t) (m + 1);
	int16_t long_mask = (int16_t) ((2 << m) - 1);
	uint8_t middle_mask = (uint8_t) (long_mask >> 1);

	int16_t levels = (int16_t) (max_sym - short_range - long_mask - 1);
	levels = (int16_t) (levels >> m);
	levels = (int16_t) (levels + 2);

	if ((uint16_t) levels >= 0x20)
		levels = 0x20;

	uint8_t level_count = (uint8_t) levels;
	uint8_t long_offset = (uint8_t) (((int16_t) (levels - 1) << m) + short_range);

	int32_t cursor = bit_pos >> 3;
	uint8_t hi = 0, lo = 0;
	uint8_t shift = (uint8_t) (0x10 - (bit_pos & 7));

	for (;;) {
		uint8_t symbol;

		if (shift <= k) {
			shift = (uint8_t) (shift + 8);
			cursor++;
		}

		hi = bits_byte(src, cursor);
		lo = bits_byte(src, cursor + 1);
		shift = (uint8_t) (shift - 1);

		if (varlen_bit(varlen_window(hi, lo), shift)) {
			shift = (uint8_t) (shift - k);
			symbol = (uint8_t) (varlen_sar(varlen_window(hi, lo), shift) & short_mask);

		} else {
			uint8_t level = (uint8_t) (level_count - 1);
			bool longest = level == 0;

			while (!longest) {
				uint8_t before = shift;
				shift = (uint8_t) (shift - 1);

				if (before < 1) {
					shift = (uint8_t) (shift + 8);
					cursor++;
					lo = bits_byte(src, cursor + 1);
				}

				if (varlen_bit(varlen_window(hi, lo), shift))
					break;

				level = (uint8_t) (level - 1);
				longest = level == 0;
			}

			if (longest) {
				uint8_t before = shift;
				shift = (uint8_t) (shift - m_plus_1);

				if (before < m_plus_1) {
					shift = (uint8_t) (shift + 8);
					cursor++;
					hi = bits_byte(src, cursor);
					lo = bits_byte(src, cursor + 1);
				}

				symbol = (uint8_t) ((varlen_sar(varlen_window(hi, lo), shift) & (uint16_t) long_mask) + long_offset);

			} else {
				uint8_t offset_lo = (uint8_t) ((uint32_t) (uint8_t) (level_count - level - 1) << (m & 31));
				uint16_t pair = (uint16_t) ((uint16_t) shift << 8 | offset_lo);
				pair = (uint16_t) (pair + (uint16_t) short_range);
				offset_lo = (uint8_t) (pair & 0xFF);

				uint8_t before = (uint8_t) (pair >> 8);
				shift = (uint8_t) (before - m);

				if (before < m) {
					shift = (uint8_t) (shift + 8);
					cursor++;
					hi = bits_byte(src, cursor);
					lo = bits_byte(src, cursor + 1);
				}

				symbol = (uint8_t) ((varlen_sar(varlen_window(hi, lo), shift) & middle_mask) + offset_lo);
			}
		}

		*out++ = symbol;

		if (--count == 0)
			break;
	}

	return cursor * 8 + 0x10 - shift;
}

static int32_t decode_bitstream_pairs(const struct bits *base, int32_t bit_pos, uint8_t n1, uint8_t n2, int32_t term,
	int32_t count, uint8_t *dst)
{
	if (count <= 0)
		return bit_pos;

	uint8_t mask1 = (uint8_t) ((1u << n1) - 1);
	uint8_t mask2 = (uint8_t) ((1u << n2) - 1);
	uint8_t sentinel = (uint8_t) term;
	int32_t cursor = bit_pos >> 3;
	int32_t shift = 0x10 - (bit_pos & 7);

	for (;;) {
		shift -= n1;

		if (shift < 0) {
			shift += 8;
			cursor++;
		}

		uint8_t symbol = (uint8_t) (varlen_sar(varlen_window(bits_byte(base, cursor), bits_byte(base, cursor + 1)),
			(uint8_t) shift) & mask1);

		if (symbol == mask1) {
			uint8_t total = symbol;

			while (total != sentinel) {
				shift -= n2;

				if (shift < 0) {
					shift += 8;
					cursor++;
				}

				uint8_t extra = (uint8_t) (varlen_sar(varlen_window(bits_byte(base, cursor),
					bits_byte(base, cursor + 1)), (uint8_t) shift) & mask2);
				total = (uint8_t) (total + extra);

				if (extra != mask2)
					break;
			}

			symbol = total;
		}

		*dst++ = symbol;

		if (--count == 0)
			break;
	}

	return cursor * 8 + 0x10 - (uint8_t) shift;
}

static int32_t decode_block_mtf1(struct decode_table *header, const struct bits *src, int32_t bit_pos, uint8_t *out,
	int32_t *out_len)
{
	*out_len = (int32_t) (window32(src, bit_pos) >> 26);
	bit_pos += 6;

	uint8_t short_width = (uint8_t) ((window32(src, bit_pos) >> 29) + 1);
	bit_pos += 3;

	uint8_t step_width = (uint8_t) ((window32(src, bit_pos) >> 29) + 1);
	bit_pos += 3;

	bit_pos = decode_varlen_bits(src, bit_pos, short_width, step_width, (int16_t) (header->count - 1), *out_len, out);

	const uint8_t *table = header->table;
	uint8_t remaining = (uint8_t) *out_len;
	uint8_t running = 0;

	for (;;) {
		running = (uint8_t) (running + *out);
		*out = table[running];
		out++;
		table++;

		if (remaining == 0 || --remaining == 0)
			break;
	}

	return bit_pos;
}

static int32_t decode_block_mtf2(struct decode_table *header, const struct bits *src, int32_t bit_pos, uint8_t *out,
	int32_t *out_len)
{
	uint8_t out_count = (uint8_t) (window32(src, bit_pos) >> 26);
	bit_pos += 6;

	uint8_t short_width = (uint8_t) ((window32(src, bit_pos) >> 29) + 1);
	bit_pos += 3;

	uint8_t step_width = (uint8_t) ((window32(src, bit_pos) >> 29) + 1);
	bit_pos += 3;

	bit_pos = decode_varlen_bits(src, bit_pos, short_width, step_width, (int16_t) (header->count - 1),
		(int16_t) out_count, out);

	const uint8_t *table = header->table;
	uint8_t *cursor = out;
	uint8_t remaining = (uint8_t) (out_count - 1);
	int32_t skew = 0;
	uint8_t running = cursor[skew];

	*cursor = table[running];
	cursor++;

	for (;;) {
		uint8_t sym = cursor[skew];

		if (sym != 0) {
			running = (uint8_t) (running + sym);
			*cursor = table[running];
			cursor++;

			if (remaining <= 1)
				break;

			remaining = (uint8_t) (remaining - 1);

		} else {
			skew++;
			running = cursor[skew];
			*cursor = table[running];
			cursor++;

			if (remaining <= 2)
				break;

			remaining = (uint8_t) (remaining - 2);
		}
	}

	*out_len = (uint8_t) (cursor - out);

	return bit_pos;
}

static int32_t decode_block_dispatch(struct decode_table *header, const struct bits *src, int32_t bit_pos,
	uint8_t **out, int32_t *out_len)
{
	if (!header->mode) {
		*out_len = header->count;
		*out = header->table;
		return bit_pos;
	}

	uint8_t value = (uint8_t) (window32(src, bit_pos) >> (32 - header->mode));
	bit_pos += header->mode;

	switch (value) {
		case 0:
			*out_len = header->count;
			*out = header->table;
			break;

		case 1:
			bit_pos = decode_block_mtf1(header, src, bit_pos, *out, out_len);
			break;

		case 2:
			bit_pos = decode_block_mtf2(header, src, bit_pos, *out, out_len);
			break;
	}

	return bit_pos;
}

static int32_t decode_64block(struct decode_table *header, const struct bits *src, int32_t bit_pos, uint8_t *dst,
	bool varlen)
{
	uint8_t lookup[256];
	uint8_t *table = lookup;
	int32_t symbol_count = 0;

	memset(lookup, 0, sizeof(lookup));
	bit_pos = decode_block_dispatch(header, src, bit_pos, &table, &symbol_count);

	uint8_t low = (uint8_t) (window16(src, bit_pos) >> 10);
	uint8_t high = (uint8_t) (((low >> 3) & 7) + 1);
	low = (uint8_t) ((low & 7) + 1);
	bit_pos += 6;

	if (varlen) {
		bit_pos = decode_varlen_bits(src, bit_pos, high, low, symbol_count - 1, 64, dst);

	} else {
		bit_pos = decode_bitstream_pairs(src, bit_pos, high, low, symbol_count - 1, 64, dst);
	}

	for (int32_t i = 63; i >= 0; i--)
		dst[i] = table[dst[i]];

	return bit_pos;
}

static int32_t decode_image_block(struct decode_table *header, const struct bits *src, int32_t bit_pos, uint8_t *dst,
	uint32_t *out_extra, int32_t depth)
{
	*out_extra = 0;

	uint8_t type = (uint8_t) (window32(src, bit_pos) >> 30);
	bit_pos += 2;

	if (type == 0)
		return decode_64block(header, src, bit_pos, dst, false);

	if (type == 1)
		return decode_64block(header, src, bit_pos, dst, true);

	if (type == 2) {
		int32_t sub_bit_pos = (int32_t) (window32(src, bit_pos) >> 8);
		bit_pos += 24;

		if (depth < 64)
			decode_image_block(header, src, sub_bit_pos, dst, out_extra, depth + 1);

		*out_extra = 0;
		return bit_pos;
	}

	header->allow_skip = true;

	uint8_t fill = (uint8_t) (window32(src, bit_pos) >> 24);
	bit_pos += 8;

	uint32_t flag = window32(src, bit_pos) >> 31;
	bit_pos++;

	if (flag) {
		*out_extra = window32(src, bit_pos) >> 16;
		bit_pos += 16;
	}

	memset(dst, fill, 64);

	return bit_pos;
}

int32_t DecodeImageBlock(struct decode_table *table, const uint8_t *src, int32_t src_len, int32_t bit_pos, uint8_t *dst,
	uint32_t *out_extra)
{
	struct bits b = {src, src_len};

	return decode_image_block(table, &b, bit_pos, dst, out_extra, 0);
}

static void decompress_image(const uint8_t *data, int32_t size, struct sink *s)
{
	if (size < 12)
		return;

	int32_t data_off = ReadLe16(data);
	int32_t flags = (int8_t) data[3];
	int32_t total_len = ReadLe32(data + 8);

	if (data_off < 0 || data_off >= size)
		return;

	const uint8_t *stream = data + data_off;

	int16_t block_count = (int16_t) ((total_len + 0x3F) / 64);

	if (block_count <= 0)
		return;

	struct decode_table header;
	memset(&header, 0, sizeof(header));

	if (flags & 1) {
		header.count = *stream + 1;

		if (data_off + 1 + header.count > size)
			return;

		stream++;
		memcpy(header.table, stream, (size_t) header.count);
		stream += header.count;
		header.mode = 2;
	}

	struct bits bits = {stream, size - (int32_t) (stream - data)};
	uint8_t pixels[64];
	int32_t bit_pos = 0;
	int32_t blocks_left = block_count;

	while (blocks_left > 0) {
		uint32_t extra = 0;
		bit_pos = decode_image_block(&header, &bits, bit_pos, pixels, &extra, 0);
		s->allow_skip = header.allow_skip;

		int32_t repeats = (int32_t) extra;

		do {
			write_block(s, pixels);
			blocks_left--;
			repeats--;
		} while (repeats >= 0 && blocks_left > 0);
	}
}

void DecodeFrame(const uint8_t *data, int32_t size, int32_t w, int32_t h, uint8_t *dst, int32_t stride, int32_t mode)
{
	struct sink s;
	memset(&s, 0, sizeof(s));
	s.mode = mode;
	s.width = w;

	if (h > 0) {
		s.height = h;
		s.dst = dst;

	} else {
		s.bottom_up = true;
		s.height = -h;
		s.dst = dst + (s.height - 1) * stride;
	}

	s.cur = s.dst;
	s.stride = stride;

	decompress_image(data, size, &s);
}

void DecodeFrameSkip(const uint8_t *data, int32_t size, int32_t w, int32_t h, uint8_t *dst, int32_t stride,
	uint8_t *drawn)
{
	struct sink s;
	memset(&s, 0, sizeof(s));
	s.mode = SINK_SKIP;
	s.width = w;
	s.height = h;
	s.dst = dst;
	s.cur = dst;
	s.stride = stride;
	s.drawn = drawn;
	s.tiles_down = (h + 7) / 8;

	decompress_image(data, size, &s);
}
