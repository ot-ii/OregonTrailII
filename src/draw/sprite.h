#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "drawable.h"
#include "../base/geom.h"

struct canvas;
struct rsrc;
struct sprite;

struct sprite_ops {
	struct drawable_ops base;
	int32_t (*Slot16)(struct sprite *self, int32_t a2, int32_t a3, int32_t a4, int32_t a5);
	int32_t (*DecodeRow)(struct sprite *self, const uint8_t **src_ptr, uint8_t *dst);
	int32_t (*DecodeClippedRow)(struct sprite *self, const uint8_t *src, uint8_t *dst, int32_t skip,
		int32_t width);
	bool (*HitTest)(struct sprite *self, uint32_t y, uint32_t x, uint32_t frame);
	int32_t (*ByteswapResource)(struct sprite *self);
};

struct sprite {
	struct drawable base;
	struct rsrc *image;
};

#define SPRITE_OPS(p) ((const struct sprite_ops *) ((const struct object *) (p))->ops)

extern const struct sprite_ops SPRITE_OPS_TABLE;

void SpriteInit(struct sprite *self, struct rsrc *image);
struct sprite *SpriteCreate(struct rsrc *image);
void SpriteDestroy(struct sprite **ctx_mod);
void sprite_destroy(struct object *self);

int32_t sprite_byteswap_resource(struct sprite *self);
int32_t sprite_slot16(struct sprite *self, int32_t a2, int32_t a3, int32_t a4, int32_t a5);
int32_t sprite_draw_at(struct drawable *self, struct canvas *dc, struct point *pos, int32_t index, int32_t y,
	int32_t flag);
int32_t sprite_decode_clipped_row(struct sprite *self, const uint8_t *src, uint8_t *dst, int32_t skip,
	int32_t width);
int32_t sprite_decode_row(struct sprite *self, const uint8_t **src_ptr, uint8_t *dst);
int32_t sprite_get_size_at(struct drawable *self, struct point *frame, int32_t b);
bool sprite_hit_test(struct sprite *self, uint32_t y, uint32_t x, uint32_t frame);

int32_t SpriteGetFrameCount(struct sprite *self);
