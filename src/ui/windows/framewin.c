#include "../framewin.h"

#include <stddef.h>

#include <windows.h>

#include "../../base/cstr.h"
#include "../../base/memory.h"
#include "../view.h"

struct frame_win *g_parent_win;

const struct window_ops FRAME_WIN_OPS_TABLE = {
	{frame_win_destroy},
	window_vf04,
	window_vf08,
	window_vf0c,
	window_post_close_event,
	window_vf14,
	window_draw,
	window_is_dialog,
	window_on_activate,
	window_on_deactivate,
	window_get_text,
	window_post_mouse_event,
	window_handle_key,
	window_handle_mouse,
};

static int32_t view_width(struct view *view)
{
	struct rect r = VIEW_OPS(view)->GetBounds(view);

	return r.right - r.left;
}

static int32_t view_height(struct view *view)
{
	struct rect r = VIEW_OPS(view)->GetBounds(view);

	return r.bottom - r.top;
}

void FrameWinInit(struct frame_win *self, struct cstr *title, struct view *root_view, struct point a4,
	bool resizable, bool modal)
{
	(void) a4;

	int32_t w = view_width(root_view);
	int32_t h = view_height(root_view);
	WindowInitSize(&self->base, root_view, w, h);
	self->base.base.ops = &FRAME_WIN_OPS_TABLE.base;
	self->resizable = resizable;
	self->modal = modal;

	uint32_t style = 0xC20000;

	if (resizable)
		style |= 0x80000;

	if (modal)
		style |= 0x50000;

	uint32_t ex_style = 0;
	WindowCreate(&self->base, 1, title, style, ex_style, NULL);

	if (title)
		CStrDestroy(&title);

	self->inherit_flag = NULL;
}

struct frame_win *FrameWinCreate(struct cstr *title, struct view *root_view, struct point a4, bool resizable,
	bool modal)
{
	struct frame_win *self = MemAlloc(1, sizeof(struct frame_win));
	FrameWinInit(self, title, root_view, a4, resizable, modal);

	return self;
}

void FrameWinDestroy(struct frame_win **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void frame_win_destroy(struct object *obj)
{
	struct frame_win *self = (struct frame_win *) obj;

	SetMenu((HWND) self->base.hwnd, NULL);

	obj->ops = &WINDOW_OPS_TABLE.base;
	window_destroy(obj);
}

void WindowSetState(struct window *self, struct doc_file *flag)
{
	if (self->root_view) {
		ViewSetInheritFlag(self->root_view, flag);

		if (self->win_type == 1)
			((struct frame_win *) self)->inherit_flag = flag;
	}
}
