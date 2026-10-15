#pragma once

#include <stdbool.h>
#include <stdint.h>

struct smc_decoder {
	int32_t width;
	int32_t height;
	int32_t stride;
	int32_t rows;
	uint8_t *pixels;
	uint8_t pairs[256][2];
	uint8_t quads[256][4];
	uint8_t octets[256][8];
};

struct smc_decoder *SmcDecoderCreate(int32_t width, int32_t height);
void SmcDecoderDestroy(struct smc_decoder **ctx_mod);
bool SmcDecoderDecode(struct smc_decoder *ctx, const uint8_t *data, uint32_t size);
