#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "drawable.h"
#include "../base/geom.h"

struct canvas;
struct multi_draw;
struct screen_dc;
struct server;

struct multi_draw_ops {
	struct drawable_ops base;
};

struct multi_draw {
	struct drawable base;
	struct drawable **slots;
};

#define MULTI_DRAW_OPS(p) ((const struct multi_draw_ops *) ((const struct object *) (p))->ops)

extern const struct multi_draw_ops MULTI_DRAW_OPS_TABLE;

void MultiDrawInit(struct multi_draw *self, struct server *arg);
struct multi_draw *MultiDrawCreate(struct server *arg);
void MultiDrawDestroy(struct multi_draw **ctx_mod);
void multi_draw_destroy(struct object *self);

int32_t multi_draw_draw(struct drawable *self, struct screen_dc *dc, struct point *pos, int32_t index,
	int32_t flag);
bool multi_draw_free(struct media_base *self);
bool multi_draw_release_lock(struct media_base *self);
bool multi_draw_is_unlocked(struct media_base *self);
int32_t multi_draw_get_size_at(struct drawable *self, struct point *out, int32_t b);
struct canvas *multi_draw_get_rect_at(struct drawable *self, int32_t a, struct rect *b);

void MultiDrawAllocSlots(struct multi_draw *self);
