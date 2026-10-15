#pragma once

#include <stdint.h>

#include "../base/geom.h"
#include "../base/object.h"

struct region;
struct screen_dc;
struct shape;

struct shape_ops {
	struct object_ops base;
	void (*DrawBrush20)(struct shape *self, struct screen_dc *dc);
	void (*DrawBrush12)(struct shape *self, struct screen_dc *dc);
	void (*DrawOutline)(struct shape *self, struct screen_dc *dc);
	void (*DrawInvert)(struct shape *self, struct screen_dc *dc);
	void (*Draw)(struct shape *self, struct screen_dc *dc);
	struct rect (*GetRectByVal)(struct shape *self);
	void (*SetRect)(struct shape *self, struct rect *r);
	struct region *(*MakeRegion)(struct shape *self);
};

struct shape {
	struct object base;
	struct rect rc;
	int32_t altBrushColor;
	int32_t penColor;
	int32_t brushColor;
	int32_t penWidth;
	int32_t penStyle;
};

#define SHAPE_OPS(p) ((const struct shape_ops *) ((const struct object *) (p))->ops)

extern const struct shape_ops SHAPE_OPS_TABLE;

void ShapeInitRect(struct shape *self, struct rect *rect, int32_t pen_color, int32_t brush_color,
	int32_t alt_brush_color, int32_t pen_width, int32_t pen_style);
void ShapeInit(struct shape *self, int32_t pen_color, int32_t brush_color, int32_t alt_brush_color,
	int32_t pen_width, int32_t pen_style);
struct shape *ShapeCreateRect(struct rect *rect, int32_t pen_color, int32_t brush_color, int32_t alt_brush_color,
	int32_t pen_width, int32_t pen_style);
struct shape *ShapeCreate(int32_t pen_color, int32_t brush_color, int32_t alt_brush_color, int32_t pen_width,
	int32_t pen_style);
void ShapeDestroy(struct shape **ctx_mod);
void shape_destroy(struct object *self);

void shape_draw_brush20(struct shape *self, struct screen_dc *dc);
void shape_draw_brush12(struct shape *self, struct screen_dc *dc);
void shape_draw_outline(struct shape *self, struct screen_dc *dc);
void shape_draw_invert(struct shape *self, struct screen_dc *dc);
void shape_draw(struct shape *self, struct screen_dc *dc);
struct rect shape_get_rect_by_val(struct shape *self);
void shape_set_rect(struct shape *self, struct rect *r);
struct region *shape_make_region(struct shape *self);
