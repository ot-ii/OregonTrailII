#pragma once

#include <stdint.h>

#include "../base/geom.h"
#include "shape.h"

struct oval_ops {
	struct shape_ops base;
};

struct oval {
	struct shape base;
};

#define OVAL_OPS(p) ((const struct oval_ops *) ((const struct object *) (p))->ops)

extern const struct oval_ops OVAL_OPS_TABLE;

void OvalInit(struct oval *self, struct rect *rect, int32_t pen_color, int32_t brush_color, int32_t alt_brush_color,
	int32_t pen_width, int32_t pen_style);
struct oval *OvalCreate(struct rect *rect, int32_t pen_color, int32_t brush_color, int32_t alt_brush_color,
	int32_t pen_width, int32_t pen_style);
void OvalDestroy(struct oval **ctx_mod);
void oval_destroy(struct object *self);

void oval_draw_brush20(struct shape *self, struct screen_dc *dc);
void oval_draw_brush12(struct shape *self, struct screen_dc *dc);
void oval_draw_outline(struct shape *self, struct screen_dc *dc);
void oval_draw_invert(struct shape *self, struct screen_dc *dc);
void oval_draw(struct shape *self, struct screen_dc *dc);
