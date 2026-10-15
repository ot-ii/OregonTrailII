#include "../shape.h"

#include <stddef.h>

#include <windows.h>

#include "../../base/memory.h"
#include "../region.h"
#include "../screendc.h"

const struct shape_ops SHAPE_OPS_TABLE = {
	{shape_destroy},
	shape_draw_brush20,
	shape_draw_brush12,
	shape_draw_outline,
	shape_draw_invert,
	shape_draw,
	shape_get_rect_by_val,
	shape_set_rect,
	shape_make_region,
};

void ShapeInitRect(struct shape *self, struct rect *rect, int32_t pen_color, int32_t brush_color,
	int32_t alt_brush_color, int32_t pen_width, int32_t pen_style)
{
	ObjectInit(&self->base);
	self->base.ops = &SHAPE_OPS_TABLE.base;
	self->penWidth = pen_width;
	self->penStyle = pen_style;
	self->rc = *rect;
	self->penColor = pen_color;
	self->brushColor = brush_color;
	self->altBrushColor = alt_brush_color;
}

void ShapeInit(struct shape *self, int32_t pen_color, int32_t brush_color, int32_t alt_brush_color,
	int32_t pen_width, int32_t pen_style)
{
	ObjectInit(&self->base);
	self->base.ops = &SHAPE_OPS_TABLE.base;
	self->penWidth = pen_width;
	self->penStyle = pen_style;
	self->rc = RectMake(0, 0, 0, 0);
	self->penColor = pen_color;
	self->brushColor = brush_color;
	self->altBrushColor = alt_brush_color;
}

struct shape *ShapeCreateRect(struct rect *rect, int32_t pen_color, int32_t brush_color, int32_t alt_brush_color,
	int32_t pen_width, int32_t pen_style)
{
	struct shape *ctx = MemAlloc(1, sizeof(struct shape));
	ShapeInitRect(ctx, rect, pen_color, brush_color, alt_brush_color, pen_width, pen_style);

	return ctx;
}

struct shape *ShapeCreate(int32_t pen_color, int32_t brush_color, int32_t alt_brush_color, int32_t pen_width,
	int32_t pen_style)
{
	struct shape *ctx = MemAlloc(1, sizeof(struct shape));
	ShapeInit(ctx, pen_color, brush_color, alt_brush_color, pen_width, pen_style);

	return ctx;
}

void ShapeDestroy(struct shape **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void shape_destroy(struct object *self)
{
	object_destroy(self);
}

void shape_set_rect(struct shape *self, struct rect *r)
{
	self->rc = *r;
}

struct rect shape_get_rect_by_val(struct shape *self)
{
	return self->rc;
}

void shape_draw_brush20(struct shape *self, struct screen_dc *dc)
{
	HDC hdc = dc->hdc;
	ScreenDcSetBrush(dc, self->brushColor);
	ScreenDcSetPen(dc, self->penColor, self->penWidth, self->penStyle);
	Rectangle(hdc, self->rc.left, self->rc.top, self->rc.right, self->rc.bottom);
}

void shape_draw_brush12(struct shape *self, struct screen_dc *dc)
{
	HDC hdc = dc->hdc;
	ScreenDcSetBrush(dc, self->altBrushColor);
	ScreenDcSetPen(dc, self->penColor, self->penWidth, self->penStyle);
	Rectangle(hdc, self->rc.left, self->rc.top, self->rc.right, self->rc.bottom);
}

void shape_draw_outline(struct shape *self, struct screen_dc *dc)
{
	if (!self->penWidth)
		return;

	HDC hdc = dc->hdc;
	ScreenDcSetPen(dc, self->penColor, self->penWidth, self->penStyle);
	HGDIOBJ old = SelectObject(hdc, GetStockObject(NULL_BRUSH));
	Rectangle(hdc, self->rc.left, self->rc.top, self->rc.right, self->rc.bottom);
	SelectObject(hdc, old);
}

void shape_draw_invert(struct shape *self, struct screen_dc *dc)
{
	HDC hdc = dc->hdc;
	RECT invert_area;
	invert_area.left = self->rc.left;
	invert_area.top = self->rc.top;
	invert_area.right = self->rc.right;
	invert_area.bottom = self->rc.bottom;
	InvertRect(hdc, &invert_area);
}

struct region *shape_make_region(struct shape *self)
{
	struct rect r = SHAPE_OPS(self)->GetRectByVal(self);

	return RegionCreateRect(&r, 1, 0, 0);
}

void shape_draw(struct shape *self, struct screen_dc *dc)
{
	HDC hdc = dc->hdc;
	ScreenDcSetBrush(dc, self->brushColor);
	ScreenDcSetPen(dc, self->brushColor, self->penWidth, self->penStyle);
	Rectangle(hdc, self->rc.left, self->rc.top, self->rc.right, self->rc.bottom);
}
