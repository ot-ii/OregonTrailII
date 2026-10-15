#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "objbase.h"

struct canvas;
struct drawable;
struct screen_dc;
struct server;

struct drawable_ops {
	struct obj_base_ops base;
	int32_t (*DrawAt)(struct drawable *self, struct canvas *dc, struct point *pos, int32_t index, int32_t y,
		int32_t flag);
	int32_t (*Draw)(struct drawable *self, struct screen_dc *dc, struct point *pos, int32_t index, int32_t flag);
	int32_t (*GetSizeAt)(struct drawable *self, struct point *out, int32_t index);
	struct canvas *(*GetRectAt)(struct drawable *self, int32_t index, struct rect *out);
};

struct drawable {
	struct obj_base base;
	int32_t item_count;
};

#define DRAWABLE_OPS(p) ((const struct drawable_ops *) ((const struct object *) (p))->ops)

extern const struct drawable_ops DRAWABLE_OPS_TABLE;

void DrawableInit(struct drawable *self, struct server *server);
void drawable_destroy(struct object *self);

int32_t drawable_draw_to(struct obj_base *self, struct screen_dc *dc, struct point *pos, int32_t flag);
int32_t drawable_draw(struct drawable *self, struct screen_dc *dc, struct point *pos, int32_t index, int32_t flag);
int32_t drawable_draw_at(struct drawable *self, struct canvas *dc, struct point *pos, int32_t index, int32_t y,
	int32_t flag);
int32_t drawable_get_size_at(struct drawable *self, struct point *out, int32_t index);
int32_t drawable_get_size(struct obj_base *self, struct point *out);
struct canvas *drawable_get_rect(struct obj_base *self, struct rect *out);
struct canvas *drawable_get_rect_at(struct drawable *self, int32_t index, struct rect *out);
