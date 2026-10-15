#include "../oval.h"

#include <windows.h>

#include "../../base/memory.h"
#include "../screendc.h"

const struct oval_ops OVAL_OPS_TABLE = {
	{
		{oval_destroy},
		oval_draw_brush20,
		oval_draw_brush12,
		oval_draw_outline,
		oval_draw_invert,
		oval_draw,
		shape_get_rect_by_val,
		shape_set_rect,
		shape_make_region,
	},
};

void OvalInit(struct oval *self, struct rect *rect, int32_t pen_color, int32_t brush_color, int32_t alt_brush_color,
	int32_t pen_width, int32_t pen_style)
{
	ShapeInitRect(&self->base, rect, pen_color, brush_color, alt_brush_color, pen_width, pen_style);
	self->base.base.ops = &OVAL_OPS_TABLE.base.base;
}

struct oval *OvalCreate(struct rect *rect, int32_t pen_color, int32_t brush_color, int32_t alt_brush_color,
	int32_t pen_width, int32_t pen_style)
{
	struct oval *ctx = MemAlloc(1, sizeof(struct oval));
	OvalInit(ctx, rect, pen_color, brush_color, alt_brush_color, pen_width, pen_style);

	return ctx;
}

void OvalDestroy(struct oval **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void oval_destroy(struct object *self)
{
	self->ops = &SHAPE_OPS_TABLE.base;
	shape_destroy(self);
}

void oval_draw_brush20(struct shape *self, struct screen_dc *dc)
{
	HDC hdc = dc->hdc;
	ScreenDcSetBrush(dc, self->brushColor);
	ScreenDcSetPen(dc, self->penColor, self->penWidth, self->penStyle);
	struct rect r = SHAPE_OPS(self)->GetRectByVal(self);
	Ellipse(hdc, r.left, r.top, r.right, r.bottom);
}

void oval_draw_brush12(struct shape *self, struct screen_dc *dc)
{
	HDC hdc = dc->hdc;
	ScreenDcSetBrush(dc, self->altBrushColor);
	ScreenDcSetPen(dc, self->penColor, self->penWidth, self->penStyle);
	struct rect r = SHAPE_OPS(self)->GetRectByVal(self);
	Ellipse(hdc, r.left, r.top, r.right, r.bottom);
}

void oval_draw_outline(struct shape *self, struct screen_dc *dc)
{
	HDC hdc = dc->hdc;
	ScreenDcSetPen(dc, self->penColor, self->penWidth, self->penStyle);
	HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
	struct rect r = SHAPE_OPS(self)->GetRectByVal(self);
	Ellipse(hdc, r.left, r.top, r.right, r.bottom);
	SelectObject(hdc, old_brush);
}

void oval_draw_invert(struct shape *self, struct screen_dc *dc)
{
	HDC hdc = dc->hdc;
	struct rect r = SHAPE_OPS(self)->GetRectByVal(self);
	HRGN oval_rgn = CreateEllipticRgn(r.left, r.top, r.right, r.bottom);
	InvertRgn(hdc, oval_rgn);
	DeleteObject(oval_rgn);
}

void oval_draw(struct shape *self, struct screen_dc *dc)
{
	HDC hdc = dc->hdc;
	ScreenDcSetBrush(dc, self->brushColor);
	ScreenDcSetPen(dc, self->brushColor, self->penWidth, self->penStyle);
	struct rect r = SHAPE_OPS(self)->GetRectByVal(self);
	Ellipse(hdc, r.left, r.top, r.right, r.bottom);
}
