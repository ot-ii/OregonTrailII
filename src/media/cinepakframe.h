#pragma once

#include <stdbool.h>
#include <stdint.h>

struct cinepak_frame;

struct cinepak_frame *CinepakFrameCreate(int32_t width, int32_t height);
void CinepakFrameDestroy(struct cinepak_frame **ctx_mod);
bool CinepakFrameDecode(struct cinepak_frame *ctx, const uint8_t *data, int32_t size);
const uint8_t *CinepakFrameRgb(const struct cinepak_frame *ctx, int32_t *stride_pixels);

struct cinepak_frame *CinepakFrameCreateEmpty(void);
bool CinepakFrameSetSize(struct cinepak_frame *ctx, int32_t width, int32_t height);
void CinepakFrameWriteOut(const struct cinepak_frame *ctx, int32_t out_width, int32_t out_height, int32_t bit_count,
	uint8_t *bits);

bool CinepakInstall(void);

#ifdef OTII_TEST
intptr_t CinepakTestDriverProc(uintptr_t id, uint32_t msg, intptr_t lp1, intptr_t lp2);
#endif
