#include "../floatwin.h"

#include <stddef.h>

#include "../../base/cstr.h"
#include "../../base/memory.h"

const struct window_ops FLOAT_WIN_OPS_TABLE = {
	{float_win_destroy},
	window_vf04,
	window_vf08,
	window_vf0c,
	window_post_close_event,
	window_vf14,
	window_draw,
	window_is_dialog,
	float_win_on_activate,
	float_win_on_deactivate,
	window_get_text,
	window_post_mouse_event,
	window_handle_key,
	window_handle_mouse,
};

void FloatWinInit(struct float_win *self, struct cstr *title, struct view *root_view, int32_t w, int32_t h,
	struct window *parent, int32_t a7)
{
	(void) a7;

	WindowInitSize(&self->base, root_view, w, h);
	self->base.base.ops = &FLOAT_WIN_OPS_TABLE.base;

	uint32_t style = 0x80C84000;
	uint32_t ex_style = 0;
	WindowCreate(&self->base, 4, title, style, ex_style, parent);

	if (title)
		CStrDestroy(&title);
}

struct float_win *FloatWinCreate(struct cstr *title, struct view *root_view, int32_t w, int32_t h,
	struct window *parent, int32_t a7)
{
	struct float_win *self = MemAlloc(1, sizeof(struct float_win));
	FloatWinInit(self, title, root_view, w, h, parent, a7);

	return self;
}

void FloatWinDestroy(struct float_win **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void float_win_destroy(struct object *self)
{
	self->ops = &WINDOW_OPS_TABLE.base;
	window_destroy(self);
}

void float_win_on_deactivate(struct window *self)
{
	(void) self;
}

void float_win_on_activate(struct window *self)
{
	window_on_activate(self);
}
