#pragma once

#include <stdbool.h>
#include <stdint.h>

#define DECODE_OPAQUE       0
#define DECODE_TRANSPARENT  1

void DecodeFrame(const uint8_t *data, int32_t size, int32_t w, int32_t h, uint8_t *dst, int32_t stride, int32_t mode);
void DecodeFrameSkip(const uint8_t *data, int32_t size, int32_t w, int32_t h, uint8_t *dst, int32_t stride,
	uint8_t *drawn);

struct decode_table {
	int32_t count;
	uint8_t table[256];
	uint8_t mode;
	bool allow_skip;
};

int32_t DecodeImageBlock(struct decode_table *table, const uint8_t *src, int32_t src_len, int32_t bit_pos, uint8_t *dst,
	uint32_t *out_extra);

int32_t DecodeOutOfRangeReads(void);
