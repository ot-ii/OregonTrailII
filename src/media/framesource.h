#pragma once

#include <stdbool.h>
#include <stdint.h>

struct frame_source;
struct image;

enum frame_status {
	FRAME_NOT_READ = 0,
	FRAME_DAMAGED = 1,
	FRAME_DECODED = 2,
};

struct frame_source *FrameSourceOpen(const char *path);
void FrameSourceDestroy(struct frame_source **ctx_mod);

int32_t FrameSourceWidth(const struct frame_source *ctx);
int32_t FrameSourceHeight(const struct frame_source *ctx);
int32_t FrameSourceFrameCount(const struct frame_source *ctx);
int32_t FrameSourceDurationMs(const struct frame_source *ctx);
int32_t FrameSourceFrameAt(const struct frame_source *ctx, int32_t ms);
int32_t FrameSourceFrameTime(const struct frame_source *ctx, int32_t index);

int32_t FrameSourceDecode(struct frame_source *ctx, int32_t index);
void FrameSourceRewind(struct frame_source *ctx);
const struct image *FrameSourceImage(struct frame_source *ctx);
const uint8_t (*FrameSourcePalette(const struct frame_source *ctx))[3];

bool FrameSourceSound(struct frame_source *ctx, uint8_t **pcm, int32_t *bytes, int32_t *rate, int32_t *channels,
	int32_t *bits);
