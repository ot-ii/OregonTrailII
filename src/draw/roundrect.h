#pragma once

#include <stdint.h>

#include "../base/geom.h"
#include "shape.h"

struct round_rect_ops {
	struct shape_ops base;
};

struct round_rect {
	struct shape base;
	int32_t corner_w;
	int32_t corner_h;
};

#define ROUND_RECT_OPS(p) ((const struct round_rect_ops *) ((const struct object *) (p))->ops)

extern const struct round_rect_ops ROUND_RECT_OPS_TABLE;

void RoundRectInit(struct round_rect *self, struct rect *rect, int32_t corner_w, int32_t corner_h,
	int32_t pen_color, int32_t brush_color, int32_t alt_brush_color, int32_t pen_width, int32_t pen_style);
struct round_rect *RoundRectCreate(struct rect *rect, int32_t corner_w, int32_t corner_h, int32_t pen_color,
	int32_t brush_color, int32_t alt_brush_color, int32_t pen_width, int32_t pen_style);
void RoundRectDestroy(struct round_rect **ctx_mod);
void round_rect_destroy(struct object *self);

void round_rect_draw_brush20(struct shape *self, struct screen_dc *dc);
void round_rect_draw_brush12(struct shape *self, struct screen_dc *dc);
void round_rect_draw_outline(struct shape *self, struct screen_dc *dc);
void round_rect_draw_invert(struct shape *self, struct screen_dc *dc);
void round_rect_draw(struct shape *self, struct screen_dc *dc);
struct region *round_rect_make_region(struct shape *self);
