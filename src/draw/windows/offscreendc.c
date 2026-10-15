#include "../offscreendc.h"

#include <stddef.h>

#include <windows.h>

#include "../canvas.h"
#include "../drawable.h"
#include "../../base/memory.h"
#include "../shape.h"

const int32_t g_rop2[8] = {
	R2_COPYPEN,
	R2_MERGEPEN,
	R2_XORPEN,
	R2_MASKNOTPEN,
	R2_NOTCOPYPEN,
	R2_MERGENOTPEN,
	R2_NOTXORPEN,
	R2_MASKPEN,
};

const uint32_t g_rop[8] = {
	SRCCOPY,
	SRCPAINT,
	SRCINVERT,
	SRCCOPY,
	NOTSRCCOPY,
	NOTSRCERASE,
	SRCCOPY,
	SRCCOPY,
};

const struct offscreen_dc_ops OFFSCREEN_DC_OPS_TABLE = {
	{
		{offscreen_dc_destroy},
		screen_dc_bit_blt,
		screen_dc_blit_canvas,
		screen_dc_draw_image,
		offscreen_dc_capture_rect,
		screen_dc_get_cursor_point,
		offscreen_dc_is_screen,
		screen_dc_set_bk_color,
		screen_dc_set_clip_region,
		screen_dc_set_clip_rect,
		screen_dc_set_color,
		offscreen_dc_sync,
	},
};

void OffscreenDcInit(struct offscreen_dc *self)
{
	ScreenDcInit(&self->base);
	self->base.base.ops = &OFFSCREEN_DC_OPS_TABLE.base.base;
}

void OffscreenDcInitCanvas(struct offscreen_dc *self, struct canvas *source, struct palette *palette_obj)
{
	OffscreenDcInit(self);

	struct screen_dc *dc = &self->base;
	dc->is_valid = 0;
	dc->hdc = CreateCompatibleDC(NULL);

	if (!dc->hdc)
		return;

	struct rect bounds = CanvasGetBounds(source);

	if (g_use_dib_sections && source->backing_dc) {
		GdiFlush();
		source->bits.bits = source->backing_dc->bits;
	}

	if (source->dib) {
		CanvasDibLocked(source);
		source->bits.bits = source->dib;
	}

	HBITMAP bmp = CreateBitmap(bounds.right - bounds.left, bounds.bottom - bounds.top, 1, (UINT) source->bits.depth,
		source->bits.bits);

	if (source->backing_dc) {
		source->bits.bits = NULL;

	} else if (source->dib) {
		CanvasDibUnlocked(source);
		source->bits.bits = NULL;
	}

	SelectObject((HDC) dc->hdc, bmp);
	ScreenDcInitDc(dc, dc->hdc, &bounds, palette_obj);
	dc->is_valid = 5;
}

void OffscreenDcInitRect(struct offscreen_dc *self, struct rect *src, struct palette *palette_obj, int32_t depth)
{
	OffscreenDcInit(self);

	struct screen_dc *dc = &self->base;
	struct rect bounds = *src;
	int32_t stride = (src->right - src->left) * depth / 8;

	struct canvas canvas;
	CanvasInit(&canvas);
	dc->is_valid = 0;

	int32_t dib_size = (src->bottom - src->top) * stride;
	uint8_t *dib = MemAlloc((size_t) (dib_size > 0 ? dib_size : 0), 1);
	bounds.left = 0;
	canvas.bits.rect = bounds;
	canvas.bits.stride = stride;
	canvas.bits.valid = 1;
	canvas.bits.bits = NULL;
	canvas.dib = dib;
	canvas.dib_size = dib_size;
	canvas.bits.depth = depth;

	dc->hdc = CreateCompatibleDC(NULL);

	if (!dc->hdc) {
		canvas_destroy(&canvas.base.base);
		return;
	}

	HDC screen_dc = GetDC(NULL);
	HBITMAP bmp = CreateCompatibleBitmap(screen_dc, bounds.right - bounds.left, bounds.bottom - bounds.top);
	ReleaseDC(NULL, screen_dc);

	SelectObject((HDC) dc->hdc, bmp);
	ScreenDcInitDc(dc, dc->hdc, &bounds, palette_obj);
	dc->is_valid = 4;

	canvas_destroy(&canvas.base.base);
}

struct offscreen_dc *OffscreenDcCreate(void)
{
	struct offscreen_dc *self = MemAlloc(1, sizeof(struct offscreen_dc));
	OffscreenDcInit(self);

	return self;
}

struct offscreen_dc *OffscreenDcCreateCanvas(struct canvas *source, struct palette *palette_obj)
{
	struct offscreen_dc *self = MemAlloc(1, sizeof(struct offscreen_dc));
	OffscreenDcInitCanvas(self, source, palette_obj);

	return self;
}

struct offscreen_dc *OffscreenDcCreateRect(struct rect *rect, struct palette *palette_obj, int32_t depth)
{
	struct offscreen_dc *self = MemAlloc(1, sizeof(struct offscreen_dc));
	OffscreenDcInitRect(self, rect, palette_obj, depth);

	return self;
}

void OffscreenDcDestroy(struct offscreen_dc **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void offscreen_dc_destroy(struct object *obj)
{
	struct offscreen_dc *self = (struct offscreen_dc *) obj;
	HDC hdc = (HDC) self->base.hdc;

	if (hdc) {
		HGDIOBJ owned_bmp = self->base.is_valid == 4 ? GetCurrentObject(hdc, OBJ_BITMAP) : NULL;
		DeleteDC(hdc);

		if (owned_bmp)
			DeleteObject(owned_bmp);
	}

	obj->ops = &SCREEN_DC_OPS_TABLE.base;
	screen_dc_destroy(obj);
}

struct canvas *offscreen_dc_capture_rect(struct screen_dc *self, struct rect *rect)
{
	if (!rect)
		return screen_dc_capture_rect(self, &self->rc);

	return screen_dc_capture_rect(self, rect);
}

void OffscreenDcInitValid(struct offscreen_dc *self, void *hdc, struct rect *bounds, struct palette *palette_obj)
{
	ScreenDcInitDc(&self->base, hdc, bounds, palette_obj);
	self->base.is_valid = 3;
}

bool offscreen_dc_is_screen(struct screen_dc *self)
{
	return false;
}

void offscreen_dc_sync(struct screen_dc *self)
{
}

void OffscreenDcRender(struct offscreen_dc *self, struct drawable *obj, int32_t index, struct point *pt,
	struct rect *clip)
{
	struct screen_dc *dc = &self->base;
	struct rect src_rect;

	struct canvas *source_canvas = DRAWABLE_OPS(obj)->GetRectAt(obj, index, &src_rect);
	struct rect dest_rect = src_rect;
	dest_rect.right = dest_rect.right - dest_rect.left;
	dest_rect.bottom = dest_rect.bottom - dest_rect.top;
	dest_rect.top = 0;
	dest_rect.left = dest_rect.top;
	dest_rect.left += pt->h;
	dest_rect.right += pt->h;
	dest_rect.top += pt->v;
	dest_rect.bottom += pt->v;

	if (dest_rect.left < clip->left) {
		src_rect.left = src_rect.left + clip->left - dest_rect.left;
		dest_rect.left = clip->left;
	}

	if (dest_rect.top < clip->top) {
		src_rect.top = src_rect.top + clip->top - dest_rect.top;
		dest_rect.top = clip->top;
	}

	if (dest_rect.right > clip->right) {
		src_rect.right = src_rect.right + clip->right - dest_rect.right;
		dest_rect.right = clip->right;
	}

	if (dest_rect.bottom > clip->bottom) {
		src_rect.bottom = src_rect.bottom + clip->bottom - dest_rect.bottom;
		dest_rect.bottom = clip->bottom;
	}

	struct canvas *capture = SCREEN_DC_OPS(dc)->CaptureRect(dc, &dest_rect);

	if (!capture)
		return;

	struct rect capture_rect = SHAPE_OPS(capture)->GetRectByVal(&capture->base);
	struct point capture_pos = PointMake(capture_rect.left, capture_rect.top);

	CANVAS_OPS(capture)->BlitImage(capture, source_canvas, &src_rect, &capture_pos, &capture_rect, false);

	struct rect moved = SHAPE_OPS(capture)->GetRectByVal(&capture->base);
	int32_t width = moved.right - moved.left;
	int32_t height = moved.bottom - moved.top;
	moved.top = pt->v;
	moved.left = pt->h;
	moved.right = pt->h + width;
	moved.bottom = pt->v + height;
	SHAPE_OPS(capture)->SetRect(&capture->base, &moved);

	SCREEN_DC_OPS(dc)->BlitCanvas(dc, capture, &capture_rect, &dest_rect, 0, NULL);
	ObjectDelete(capture);
}
