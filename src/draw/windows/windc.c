#include "../windc.h"

#include <windows.h>

#include "../canvas.h"
#include "../../base/memory.h"
#include "../../app/replay.h"
#include "../../ui/scaler.h"

const struct win_dc_ops WIN_DC_OPS_TABLE = {
	{
		{win_dc_destroy},
		screen_dc_bit_blt,
		screen_dc_blit_canvas,
		screen_dc_draw_image,
		screen_dc_capture_rect,
		win_dc_get_cursor_point,
		screen_dc_is_screen,
		screen_dc_set_bk_color,
		screen_dc_set_clip_region,
		screen_dc_set_clip_rect,
		screen_dc_set_color,
		screen_dc_sync,
	},
};

void WinDcInit(struct win_dc *self)
{
	ScreenDcInit(&self->base);
	self->base.base.ops = &WIN_DC_OPS_TABLE.base.base;

	self->owns_dc = false;
}

struct win_dc *WinDcCreate(void)
{
	struct win_dc *self = MemAlloc(1, sizeof(struct win_dc));
	WinDcInit(self);

	return self;
}

void WinDcDestroy(struct win_dc **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void win_dc_destroy(struct object *self)
{
	self->ops = &SCREEN_DC_OPS_TABLE.base;
	screen_dc_destroy(self);
}

void WinDcBlitCanvasGuarded(struct win_dc *self, struct canvas *image, struct rect *src_rect, struct rect *dst_rect,
	int32_t mode, struct rect *clip)
{
	if (!image->bits.valid)
		return;

	screen_dc_blit_canvas(&self->base, image, src_rect, dst_rect, mode, clip);
}

bool WinDcGetOwnsDc(struct win_dc *self)
{
	return self->owns_dc;
}

void WinDcSetOwnsDc(struct win_dc *self, bool v)
{
	self->owns_dc = v;
}

struct point win_dc_get_cursor_point(struct screen_dc *self)
{
	struct point cursor = {0};

	RpGetCursorPos(&cursor);
	ScalerScreenToDc(self->hdc, &cursor);

	return cursor;
}

bool WinDcReturnsOne(struct win_dc *self, int32_t a1, int32_t a2)
{
	return true;
}

void WinDcInitDc(struct win_dc *self, void *dc, struct rect *bounds, struct palette *palette_obj, bool owns)
{
	self->owns_dc = owns;
	ScreenDcInitDc(&self->base, dc, bounds, palette_obj);
	self->base.is_valid = 2;
}
