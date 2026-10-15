#include "../screendc.h"

#include <stddef.h>
#include <string.h>

#include <windows.h>

#include "../canvas.h"
#include "../font.h"
#include "../fonttable.h"
#include "../../base/list.h"
#include "../../base/memory.h"
#include "../offscreendc.h"
#include "../palette.h"
#include "../region.h"
#include "../../app/replay.h"
#include "../../ui/scaler.h"

#define BMI_BUF_SIZE 0x428

const struct screen_dc_ops SCREEN_DC_OPS_TABLE = {
	{screen_dc_destroy},
	screen_dc_bit_blt,
	screen_dc_blit_canvas,
	screen_dc_draw_image,
	screen_dc_capture_rect,
	screen_dc_get_cursor_point,
	screen_dc_is_screen,
	screen_dc_set_bk_color,
	screen_dc_set_clip_region,
	screen_dc_set_clip_rect,
	screen_dc_set_color,
	screen_dc_sync,
};

static void canvas_lock_bits(struct canvas *image)
{
	if (g_use_dib_sections && image->backing_dc) {
		GdiFlush();
		image->bits.bits = image->backing_dc->bits;
	}

	if (image->dib) {
		CanvasDibLocked(image);
		image->bits.bits = image->dib;
	}
}

static void canvas_unlock_bits(struct canvas *image)
{
	if (image->backing_dc) {
		image->bits.bits = NULL;

	} else if (image->dib) {
		CanvasDibUnlocked(image);
		image->bits.bits = NULL;
	}
}

void ScreenDcDrawImageIfValid(struct screen_dc *self, struct canvas *image, struct point *pos, int32_t mode)
{
	if (!image->bits.valid)
		return;

	screen_dc_draw_image(self, image, pos, mode);
}

void ScreenDcInit(struct screen_dc *self)
{
	memset(self, 0, sizeof(struct screen_dc));
	ObjectInit(&self->base);
	self->base.ops = &SCREEN_DC_OPS_TABLE.base;

	self->is_valid = 0;
	++g_dc_count;
}

struct screen_dc *ScreenDcCreate(void)
{
	struct screen_dc *self = MemAlloc(1, sizeof(struct screen_dc));
	ScreenDcInit(self);

	return self;
}

void ScreenDcDestroy(struct screen_dc **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void ScreenDcReleaseObjects(struct screen_dc *self)
{
	HDC hdc = (HDC) self->hdc;

	SelectObject(hdc, GetStockObject(BLACK_PEN));
	SelectObject(hdc, GetStockObject(BLACK_BRUSH));
	SelectPalette(hdc, (HPALETTE) GetStockObject(DEFAULT_PALETTE), FALSE);
	DeleteObject((HGDIOBJ) self->pen);
	DeleteObject((HGDIOBJ) self->brush);
}

void screen_dc_destroy(struct object *obj)
{
	struct screen_dc *self = (struct screen_dc *) obj;

	ScreenDcReleaseObjects(self);
	MemFree(self->bmi_buf);
	self->bmi_buf = NULL;
	--g_dc_count;

	object_destroy(obj);
}

void screen_dc_bit_blt(struct screen_dc *self, struct canvas *src, struct rect *src_rect, struct rect *dst_rect,
	int32_t mode, int32_t unused)
{
	BitBlt((HDC) self->hdc, dst_rect->left, dst_rect->top, dst_rect->right - dst_rect->left,
		dst_rect->bottom - dst_rect->top, (HDC) src->bits.bits, src_rect->left, src_rect->top, g_rop[mode]);
}

void screen_dc_draw_image(struct screen_dc *self, struct canvas *image, struct point *pos, int32_t mode)
{
	if (image->bits.valid) {
		struct rect src_bounds = CanvasGetBounds(image);
		struct rect dst_rect = src_bounds;
		int32_t dst_top = pos->v;
		int32_t dst_left = pos->h;
		int32_t width = dst_rect.right - dst_rect.left;
		int32_t height = dst_rect.bottom - dst_rect.top;
		dst_rect.top = dst_top;
		dst_rect.left = dst_left;
		dst_rect.right = dst_left + width;
		dst_rect.bottom = dst_top + height;
		SCREEN_DC_OPS(self)->BlitCanvas(self, image, &src_bounds, &dst_rect, mode, &self->rc);
	}
}

static void blit_strip(struct screen_dc *self, HDC mem_dc, struct canvas *image, int32_t stride, int32_t rows,
	const void *bits, struct rect *dst_rect, int32_t dst_y, int32_t dst_h, struct rect *src_rect, int32_t mode)
{
	HBITMAP strip_bitmap = CreateBitmap(stride, rows, 1, (UINT) image->bits.depth, bits);
	HGDIOBJ old_bitmap = SelectObject(mem_dc, strip_bitmap);
	BitBlt((HDC) self->hdc, dst_rect->left, dst_y, dst_rect->right - dst_rect->left, dst_h, mem_dc, src_rect->left,
		0, g_rop[mode]);
	SelectObject(mem_dc, old_bitmap);
	DeleteObject(strip_bitmap);
}

void screen_dc_blit_canvas(struct screen_dc *self, struct canvas *image, struct rect *src_rect,
	struct rect *dst_rect, int32_t mode, struct rect *clip)
{
	HDC hdc = (HDC) self->hdc;

	if (!image->bits.valid)
		return;

	struct rect image_bounds = CanvasGetBounds(image);
	int32_t stride = image->bits.stride;
	int32_t src_h = src_rect->bottom - src_rect->top;

	if (self->high_color) {
		HDC mem_dc = CreateCompatibleDC(hdc);
		int32_t area = stride * (image_bounds.bottom - image_bounds.top);

		if (area < 0x10000) {
			canvas_lock_bits(image);
			blit_strip(self, mem_dc, image, stride, src_h, image->bits.bits + stride * src_rect->top, dst_rect,
				dst_rect->top, dst_rect->bottom - dst_rect->top, src_rect, mode);

		} else {
			canvas_lock_bits(image);
			uint8_t *strip_bits = image->bits.bits;
			int32_t strip_height = 0x10000 / stride;
			int32_t strip_count = src_h / strip_height;
			uint16_t strip_dst_y = (uint16_t) dst_rect->top;
			strip_bits = strip_bits + stride * src_rect->top;

			for (int32_t strip_index = 0; strip_index < strip_count; ++strip_index) {
				blit_strip(self, mem_dc, image, stride, strip_height, strip_bits, dst_rect, strip_dst_y,
					strip_height, src_rect, mode);
				strip_bits = strip_bits + stride * strip_height;
				strip_dst_y = (uint16_t) (strip_height + strip_dst_y);
			}

			strip_height = src_h % strip_height;

			if (strip_height)
				blit_strip(self, mem_dc, image, stride, strip_height, strip_bits, dst_rect, strip_dst_y,
					strip_height, src_rect, mode);
		}

		canvas_unlock_bits(image);
		DeleteDC(mem_dc);

	} else {
		BITMAPINFOHEADER *bmi = (BITMAPINFOHEADER *) self->bmi_buf;
		canvas_lock_bits(image);
		uint8_t *dib_bits = image->bits.bits;
		int32_t area = stride * (image_bounds.bottom - image_bounds.top);

		if (area < 0x10000) {
			bmi->biWidth = stride;
			bmi->biHeight = image_bounds.bottom - image_bounds.top;
			StretchDIBits(hdc, dst_rect->left, dst_rect->top, dst_rect->right - dst_rect->left,
				dst_rect->bottom - dst_rect->top, src_rect->left, src_rect->bottom + 1,
				src_rect->right - src_rect->left, -src_h, dib_bits, (BITMAPINFO *) bmi, DIB_PAL_COLORS,
				g_rop[mode]);

		} else {
			int32_t dib_strip_height = 0x10000 / stride;
			int32_t dib_strip_count = src_h / dib_strip_height;
			uint16_t dib_dst_y = (uint16_t) dst_rect->top;
			dib_bits = dib_bits + stride * src_rect->top;
			bmi->biWidth = stride;

			if (dib_strip_count) {
				bmi->biHeight = dib_strip_height;

				for (int32_t dib_strip_index = 0; dib_strip_count > dib_strip_index; ++dib_strip_index) {
					StretchDIBits(hdc, dst_rect->left, dib_dst_y, dst_rect->right - dst_rect->left,
						dib_strip_height, src_rect->left, dib_strip_height + 1, src_rect->right - src_rect->left,
						-dib_strip_height, dib_bits, (BITMAPINFO *) bmi, DIB_PAL_COLORS, g_rop[mode]);
					dib_bits = dib_bits + stride * dib_strip_height;
					dib_dst_y = (uint16_t) (dib_dst_y + dib_strip_height);
				}
			}

			dib_strip_height = src_h % dib_strip_height;

			if (dib_strip_height) {
				bmi->biHeight = dib_strip_height;
				StretchDIBits(hdc, dst_rect->left, dib_dst_y, dst_rect->right - dst_rect->left, dib_strip_height,
					src_rect->left, dib_strip_height + 1, src_rect->right - src_rect->left, -dib_strip_height,
					dib_bits, (BITMAPINFO *) bmi, DIB_PAL_COLORS, g_rop[mode]);
			}
		}

		canvas_unlock_bits(image);
	}
}

struct region *ScreenDcGetClip(struct screen_dc *self)
{
	HDC hdc = (HDC) self->hdc;

	int32_t height = GetDeviceCaps(hdc, VERTRES);
	int32_t width = GetDeviceCaps(hdc, HORZRES);
	POINT point;
	struct rect bounds = RectMake(0, 0, height, width);
	GetViewportOrgEx(hdc, &point);
	int32_t offset_y = point.y;
	int32_t offset_x = point.x;
	bounds.left += offset_x;
	bounds.right += offset_x;
	bounds.top += offset_y;
	bounds.bottom += offset_y;

	struct region *region = RegionCreateRect(&bounds, 1, 0, 0);
	region->state = 5;

	return region;
}

struct canvas *screen_dc_capture_rect(struct screen_dc *self, struct rect *src_rect)
{
	HDC hdc = (HDC) self->hdc;
	struct canvas *canvas = NULL;

	if (self->is_valid) {
		struct rect rect = RectMake(0, 0, 0, 0);
		rect.right = src_rect->right - src_rect->left;
		rect.bottom = src_rect->bottom - src_rect->top;
		canvas = CanvasCreateRect(&rect, 8, 0xFF);

		int32_t w = rect.right - rect.left;
		int32_t h = rect.bottom - rect.top;
		HBITMAP bitmap = CreateCompatibleBitmap(hdc, w, h);
		HDC mem_dc = CreateCompatibleDC(hdc);
		SelectObject(mem_dc, bitmap);
		BitBlt(hdc, rect.left, rect.top, w, h, mem_dc, src_rect->left, src_rect->top, g_rop[0]);
		canvas_lock_bits(canvas);
		GetBitmapBits(bitmap, w * h, canvas->bits.bits);
		canvas_unlock_bits(canvas);
		DeleteDC(mem_dc);
		DeleteObject(bitmap);
	}

	return canvas;
}

void ScreenDcGetTextMetrics(struct screen_dc *self, int32_t out[4], int32_t font)
{
	TEXTMETRICA metrics;

	int32_t old_font = self->font_id;

	if (self->font_id != font)
		ScreenDcSelectFont(self, font);

	GetTextMetricsA((HDC) self->hdc, &metrics);
	out[0] = metrics.tmAscent;
	out[1] = metrics.tmDescent;
	out[2] = metrics.tmExternalLeading;
	out[3] = metrics.tmHeight;

	if (old_font != font)
		ScreenDcSelectFont(self, old_font);
}

struct point screen_dc_get_cursor_point(struct screen_dc *self)
{
	struct point pt = {0};
	RpGetCursorPos(&pt);

	return pt;
}

struct point ScreenDcPointToLocal(struct screen_dc *self, struct point *pt)
{
	struct point pos = *pt;
	ScalerScreenToDc(self->hdc, &pos);

	return pos;
}

struct rect ScreenDcScreenToClient(struct screen_dc *self, struct rect *r)
{
	struct rect rect = *r;
	struct point tl = PointMake(rect.left, rect.top);
	struct point br = PointMake(rect.right, rect.bottom);
	ScalerScreenToDc(self->hdc, &tl);
	ScalerScreenToDc(self->hdc, &br);
	rect.left = tl.h;
	rect.top = tl.v;
	rect.right = br.h;
	rect.bottom = br.v;

	return rect;
}

void ScreenDcInitDc(struct screen_dc *self, void *dc, struct rect *bounds, struct palette *pal)
{
	self->is_valid = 1;
	self->hdc = dc;
	HDC hdc = (HDC) dc;

	int32_t raster_caps = GetDeviceCaps(hdc, RASTERCAPS);
	int32_t palette_size = GetDeviceCaps(hdc, SIZEPALETTE);

	if ((raster_caps & RC_BITBLT) && (raster_caps & RC_PALETTE) && palette_size == 256) {
		self->high_color = true;

	} else {
		self->high_color = false;
	}

	self->rc = *bounds;
	SetViewportOrgEx(hdc, self->rc.left, self->rc.top, NULL);
	self->draw_mode = 8;
	self->pen_width = 1;
	self->pen_width2 = self->pen_width;
	self->palette_obj = pal;

	if (pal) {
		self->hpal = pal->hpal;
		SelectPalette(hdc, (HPALETTE) self->hpal, FALSE);
		RealizePalette(hdc);
		SelectPalette(hdc, (HPALETTE) GetStockObject(DEFAULT_PALETTE), FALSE);
		UnrealizeObject((HGDIOBJ) self->hpal);
		SelectPalette(hdc, (HPALETTE) self->hpal, FALSE);
		RealizePalette(hdc);
	}

	self->text_color = 0;
	self->bk_color = 0xFF;
	self->pen_color = 0;
	self->pen_dashed = 1;
	COLORREF colorref = ScreenDcColorToColorref(self, self->pen_color);
	self->pen = CreatePen(PS_SOLID, self->pen_width, colorref);
	SelectObject(hdc, (HGDIOBJ) self->pen);
	self->brush_color = 0;
	colorref = ScreenDcColorToColorref(self, self->brush_color);
	self->brush = CreateSolidBrush(colorref);
	SelectObject(hdc, (HGDIOBJ) self->brush);
	ScreenDcSelectFont(self, 1);
	SetBkMode(hdc, TRANSPARENT);

	MemFree(self->bmi_buf);
	self->bmi_buf_size = BMI_BUF_SIZE;
	self->bmi_buf = MemAlloc(BMI_BUF_SIZE, 1);

	BITMAPINFOHEADER *bmi = (BITMAPINFOHEADER *) self->bmi_buf;
	bmi->biSize = sizeof(BITMAPINFOHEADER);
	bmi->biWidth = 0;
	bmi->biHeight = 0;
	bmi->biPlanes = 1;
	bmi->biBitCount = 8;
	bmi->biCompression = 0;
	bmi->biSizeImage = 0;
	bmi->biXPelsPerMeter = 0;
	bmi->biYPelsPerMeter = 0;
	bmi->biClrUsed = 0;
	bmi->biClrImportant = 0;

	uint16_t *indexes = (uint16_t *) (self->bmi_buf + sizeof(BITMAPINFOHEADER));

	for (int32_t i = 0; i < 256; i++)
		indexes[i] = (uint16_t) i;
}

void ScreenDcLinePoints(struct screen_dc *self, struct point *from, struct point *to)
{
	MoveToEx((HDC) self->hdc, from->h, from->v, NULL);
	LineTo((HDC) self->hdc, to->h, to->v);
}

void ScreenDcLineCoords(struct screen_dc *self, int32_t v1, int32_t h1, int32_t v2, int32_t h2)
{
	MoveToEx((HDC) self->hdc, h1, v1, NULL);
	LineTo((HDC) self->hdc, h2, v2);
}

struct point ScreenDcPointToScreen(struct screen_dc *self, struct point *pt)
{
	struct point pos = *pt;
	ScalerDcToScreen(self->hdc, &pos);

	return pos;
}

struct rect ScreenDcRectToGlobal(struct screen_dc *self, struct rect *src)
{
	struct rect rect = *src;
	struct point tl = PointMake(rect.left, rect.top);
	struct point br = PointMake(rect.right, rect.bottom);
	ScalerDcToScreen(self->hdc, &tl);
	ScalerDcToScreen(self->hdc, &br);
	rect.left = tl.h;
	rect.top = tl.v;
	rect.right = br.h;
	rect.bottom = br.v;

	return rect;
}

bool screen_dc_is_screen(struct screen_dc *self)
{
	return true;
}

int32_t ScreenDcGetPenWidth(struct screen_dc *self)
{
	return self->pen_width2;
}

void ScreenDcSetPenWidth(struct screen_dc *self, int32_t w)
{
	self->pen_width = w;
	self->pen_width2 = self->pen_width;
	ScreenDcSetPen(self, self->pen_color, w, self->pen_dashed);
}

void ScreenDcFillRect(struct screen_dc *self, struct rect *rect, int32_t mode)
{
	Rectangle((HDC) self->hdc, rect->left, rect->top, rect->right, rect->bottom);
}

void ScreenDcFrameRect(struct screen_dc *self, struct rect *rect, int32_t mode)
{
	HGDIOBJ old = SelectObject((HDC) self->hdc, GetStockObject(NULL_BRUSH));
	Rectangle((HDC) self->hdc, rect->left, rect->top, rect->right, rect->bottom);
	SelectObject((HDC) self->hdc, old);
}

void ScreenDcInvertRect(struct screen_dc *self, struct rect *rect, int32_t mode)
{
	RECT invert_area;
	invert_area.left = rect->left;
	invert_area.top = rect->top;
	invert_area.right = rect->right;
	invert_area.bottom = rect->bottom;
	InvertRect((HDC) self->hdc, &invert_area);
}

void ScreenDcScroll(struct screen_dc *self, struct rect *rect, int32_t dx, int32_t dy, int32_t mode)
{
	RECT scroll_area;
	scroll_area.left = rect->left;
	scroll_area.top = rect->top;
	scroll_area.right = rect->right;
	scroll_area.bottom = rect->bottom;
	ScrollDC((HDC) self->hdc, dx, dy, &scroll_area, &scroll_area, NULL, NULL);
}

void screen_dc_set_bk_color(struct screen_dc *self, int32_t color)
{
	COLORREF cref = ScreenDcColorToColorref(self, color);
	self->bk_color = color;
	ScreenDcSetBrush(self, color);
	ScreenDcSetPen(self, color, self->pen_width, self->pen_dashed);
	SetBkColor((HDC) self->hdc, cref);
}

void ScreenDcSetBrush(struct screen_dc *self, int32_t color)
{
	COLORREF colorref = ScreenDcColorToColorref(self, color);

	if (self->brush_color == color)
		return;

	self->brush_color = color;
	self->brush = CreateSolidBrush(colorref);
	HGDIOBJ old_brush = SelectObject((HDC) self->hdc, (HGDIOBJ) self->brush);
	DeleteObject(old_brush);
}

void screen_dc_set_clip_rect(struct screen_dc *self, struct rect *rect)
{
	RECT clip_area;
	clip_area.left = rect->left;
	clip_area.top = rect->top;
	clip_area.right = rect->right;
	clip_area.bottom = rect->bottom;
	HRGN clip_rgn = CreateRectRgnIndirect(&clip_area);
	self->rc = *rect;
	SelectClipRgn((HDC) self->hdc, clip_rgn);

	DeleteObject(clip_rgn);
}

void screen_dc_set_clip_region(struct screen_dc *self, struct region *rgn)
{
	if (rgn->hrgn)
		SelectClipRgn((HDC) self->hdc, (HRGN) rgn->hrgn);
}

void ScreenDcSelectPalette(struct screen_dc *self, struct palette *pal)
{
	HPALETTE h = (HPALETTE) pal->hpal;

	if (h) {
		SelectPalette((HDC) self->hdc, (HPALETTE) pal->hpal, FALSE);
		RealizePalette((HDC) self->hdc);
		self->palette_obj = pal;
	}
}

void ScreenDcSelectFont(struct screen_dc *self, int32_t font_id)
{
	struct font *font = NULL;

	if (g_fonts_inited)
		font = FontTableFindById(g_font_table, font_id);

	if (font) {
		self->font_id = font_id;
		SelectObject((HDC) self->hdc, (HGDIOBJ) font->hfont);

	} else {
		self->font_id = 0;
		SelectObject((HDC) self->hdc, GetStockObject(SYSTEM_FONT));
	}
}

void screen_dc_set_color(struct screen_dc *self, int32_t color)
{
	COLORREF cref = ScreenDcColorToColorref(self, color);
	self->text_color = color;
	ScreenDcSetBrush(self, color);
	ScreenDcSetPen(self, color, self->pen_width, self->pen_dashed);
	SetTextColor((HDC) self->hdc, cref);
}

void ScreenDcSetPen(struct screen_dc *self, int32_t color, int32_t width, int32_t dashed)
{
	COLORREF colorref = ScreenDcColorToColorref(self, color);

	if (self->pen_color == color && self->pen_width == width && self->pen_dashed == dashed)
		return;

	self->pen_color = color;
	self->pen_width2 = width;
	self->pen_width = self->pen_width2;
	self->pen_dashed = dashed;

	if (self->pen_dashed) {
		self->pen = CreatePen(PS_INSIDEFRAME, self->pen_width, colorref);

	} else {
		self->pen = CreatePen(PS_SOLID, self->pen_width, colorref);
	}

	HGDIOBJ old_pen = SelectObject((HDC) self->hdc, (HGDIOBJ) self->pen);
	DeleteObject(old_pen);
}

void ScreenDcSetDrawMode(struct screen_dc *self, int32_t mode)
{
	self->draw_mode = mode;
	SetROP2((HDC) self->hdc, g_rop2[mode]);
}

void ScreenDcSetPenWidth2(struct screen_dc *self, int32_t w)
{
	self->pen_width = w;
	self->pen_width2 = self->pen_width;
	ScreenDcSetPen(self, self->pen_color, w, self->pen_dashed);
}

void screen_dc_sync(struct screen_dc *self)
{
}

uint32_t ScreenDcColorToColorref(struct screen_dc *self, int32_t color)
{
	if (color & 0x10000000)
		return PALETTERGB(color & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF);

	return (uint32_t) (uint16_t) color | 0x1000000;
}

void ScreenDcInitValid(struct screen_dc *self, void *hdc, struct rect *bounds, struct palette *palette_obj)
{
	ScreenDcInitDc(self, hdc, bounds, palette_obj);
	self->is_valid = 3;
}
