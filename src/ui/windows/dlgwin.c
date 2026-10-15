#include "../dlgwin.h"

#include <stddef.h>

#include "../../base/cstr.h"
#include "../../base/memory.h"
#include "../view.h"

const struct window_ops DLG_WIN_OPS_TABLE = {
	{dlg_win_destroy},
	window_vf04,
	window_vf08,
	window_vf0c,
	window_post_close_event,
	window_vf14,
	window_draw,
	dlg_win_is_dialog,
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

void DlgWinInit(struct dlg_win *self, struct cstr *title, struct view *root_view, struct window *parent, bool a5)
{
	int32_t w = view_width(root_view);
	int32_t h = view_height(root_view);
	WindowInitSize(&self->base, root_view, w, h);
	self->base.base.ops = &DLG_WIN_OPS_TABLE.base;
	self->unused_38 = a5;

	self->unused_38 = true;
	uint32_t style = 0x80C80000;
	uint32_t ex_style = 1;
	WindowCreate(&self->base, 2, title, style, ex_style, parent);

	if (title)
		CStrDestroy(&title);
}

struct dlg_win *DlgWinCreate(struct cstr *title, struct view *root_view, struct window *parent, bool a5)
{
	struct dlg_win *self = MemAlloc(1, sizeof(struct dlg_win));
	DlgWinInit(self, title, root_view, parent, a5);

	return self;
}

void DlgWinDestroy(struct dlg_win **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void dlg_win_destroy(struct object *self)
{
	self->ops = &WINDOW_OPS_TABLE.base;
	window_destroy(self);
}

bool dlg_win_is_dialog(struct window *self)
{
	(void) self;

	return true;
}
