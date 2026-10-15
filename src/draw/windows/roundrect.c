#include "../roundrect.h"

#include <windows.h>

#include "../../base/memory.h"
#include "../region.h"
#include "../screendc.h"

const struct round_rect_ops ROUND_RECT_OPS_TABLE = {
	{
		{round_rect_destroy},
		round_rect_draw_brush20,
		round_rect_draw_brush12,
		round_rect_draw_outline,
		round_rect_draw_invert,
		round_rect_draw,
		shape_get_rect_by_val,
		shape_set_rect,
		round_rect_make_region,
	},
};

void RoundRectInit(struct round_rect *self, struct rect *rect, int32_t corner_w, int32_t corner_h,
	int32_t pen_color, int32_t brush_color, int32_t alt_brush_color, int32_t pen_width, int32_t pen_style)
{
	ShapeInitRect(&self->base, rect, pen_color, brush_color, alt_brush_color, pen_width, pen_style);
	self->base.base.ops = &ROUND_RECT_OPS_TABLE.base.base;
	self->corner_w = corner_w;
	self->corner_h = corner_h;
}

struct round_rect *RoundRectCreate(struct rect *rect, int32_t corner_w, int32_t corner_h, int32_t pen_color,
	int32_t brush_color, int32_t alt_brush_color, int32_t pen_width, int32_t pen_style)
{
	struct round_rect *ctx = MemAlloc(1, sizeof(struct round_rect));
	RoundRectInit(ctx, rect, corner_w, corner_h, pen_color, brush_color, alt_brush_color, pen_width, pen_style);

	return ctx;
}

void RoundRectDestroy(struct round_rect **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void round_rect_destroy(struct object *self)
{
	self->ops = &SHAPE_OPS_TABLE.base;
	shape_destroy(self);
}

void round_rect_draw_brush20(struct shape *self, struct screen_dc *dc)
{
	struct round_rect *rr = (struct round_rect *) self;
	HDC hdc = dc->hdc;
	ScreenDcSetBrush(dc, self->brushColor);
	ScreenDcSetPen(dc, self->penColor, self->penWidth, self->penStyle);
	struct rect r = SHAPE_OPS(self)->GetRectByVal(self);
	RoundRect(hdc, r.left, r.top, r.right, r.bottom, rr->corner_h - 2, rr->corner_w - 2);
}

void round_rect_draw_brush12(struct shape *self, struct screen_dc *dc)
{
	struct round_rect *rr = (struct round_rect *) self;
	HDC hdc = dc->hdc;
	ScreenDcSetBrush(dc, self->altBrushColor);
	ScreenDcSetPen(dc, self->penColor, self->penWidth, self->penStyle);
	struct rect r = SHAPE_OPS(self)->GetRectByVal(self);
	RoundRect(hdc, r.left, r.top, r.right, r.bottom, rr->corner_h - 2, rr->corner_w - 2);
}

void round_rect_draw_outline(struct shape *self, struct screen_dc *dc)
{
	struct round_rect *rr = (struct round_rect *) self;
	HDC hdc = dc->hdc;
	ScreenDcSetPen(dc, self->penColor, self->penWidth, self->penStyle);
	HGDIOBJ old_obj = SelectObject(hdc, GetStockObject(NULL_BRUSH));
	struct rect r = SHAPE_OPS(self)->GetRectByVal(self);
	RoundRect(hdc, r.left, r.top, r.right, r.bottom, rr->corner_h - 2, rr->corner_w - 2);
	SelectObject(hdc, old_obj);
}

void round_rect_draw_invert(struct shape *self, struct screen_dc *dc)
{
	struct round_rect *rr = (struct round_rect *) self;
	HDC hdc = dc->hdc;
	struct rect r = SHAPE_OPS(self)->GetRectByVal(self);
	HRGN hrgn = CreateRoundRectRgn(r.left, r.top, r.right, r.bottom, rr->corner_h - 2, rr->corner_w - 2);
	InvertRgn(hdc, hrgn);
	DeleteObject(hrgn);
}

struct region *round_rect_make_region(struct shape *self)
{
	struct round_rect *rr = (struct round_rect *) self;
	struct rect r = SHAPE_OPS(self)->GetRectByVal(self);

	return RegionCreateRect(&r, 2, rr->corner_h, rr->corner_w);
}

void round_rect_draw(struct shape *self, struct screen_dc *dc)
{
	struct round_rect *rr = (struct round_rect *) self;
	HDC hdc = dc->hdc;
	ScreenDcSetBrush(dc, self->brushColor);
	ScreenDcSetPen(dc, self->brushColor, self->penWidth, self->penStyle);
	struct rect r = SHAPE_OPS(self)->GetRectByVal(self);
	RoundRect(hdc, r.left, r.top, r.right, r.bottom, rr->corner_h - 2, rr->corner_w - 2);
}
