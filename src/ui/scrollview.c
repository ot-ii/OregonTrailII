#include "scrollview.h"

#include <stddef.h>

#include "../draw/canvas.h"
#include "../base/memory.h"
#include "../draw/screendc.h"
#include "../draw/windc.h"
#include "window.h"

const struct scroll_view_ops SCROLL_VIEW_OPS_TABLE = {
	{
		{
			{scroll_view_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			scroll_view_on_event,
			view_on_idle,
			scroll_view_on_idle_spare,
		},
		view_add_child,
		view_get_bounds,
		view_get_clip_region,
		view_hide,
		view_remove_child,
		view_remove_from_parent,
		view_set_cursor_id,
		view_set_device,
		view_show,
		scroll_view_draw_children,
		view_move_children,
	},
};

void ScrollViewInit(struct scroll_view *self, int32_t id, const struct rect *rect, struct canvas *content,
	const struct rect *content_rect, bool auto_start)
{
	ViewInit(&self->base, EVCLS_SCROLLVIEW, id, rect);
	self->base.base.base.ops = &SCROLL_VIEW_OPS_TABLE.base.base.base;

	self->interval = 3;
	self->content = content;
	self->content_h = content_rect->bottom - content_rect->top;

	int32_t width = rect->right - rect->left;
	int32_t height = rect->bottom - rect->top;
	int32_t last_row = rect->bottom - rect->top - 1;
	self->scroll_rect.top = last_row;
	self->scroll_rect.left = 0;
	self->scroll_rect.right = width;
	self->scroll_rect.bottom = height;

	int32_t handler_code = CMD_SCROLLVIEW_CTRL;
	EvtHandlerBindHandler(&self->base.base, 1, &handler_code);

	if (auto_start)
		EvtHandlerAddIdleHandler(&self->base.base, false, self->interval);
}

struct scroll_view *ScrollViewCreate(int32_t id, const struct rect *rect, struct canvas *content,
	const struct rect *content_rect, bool auto_start)
{
	struct scroll_view *self = MemAlloc(1, sizeof(struct scroll_view));
	ScrollViewInit(self, id, rect, content, content_rect, auto_start);

	return self;
}

void ScrollViewDestroy(struct scroll_view **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void scroll_view_destroy(struct object *self)
{
	struct scroll_view *v = (struct scroll_view *) self;

	ObjectDestroy(&v->content);

	self->ops = &VIEW_OPS_TABLE.base.base;
	view_destroy(self);
}

void scroll_view_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds)
{
	struct scroll_view *v = (struct scroll_view *) self;
	struct rect dst = self->rect_b;
	struct rect src = v->scroll_rect;
	int32_t offset = src.bottom - (self->rect_b.bottom - self->rect_b.top);

	if (offset < 0) {
		src.top = v->content_h + offset;
		src.bottom = v->content_h;
		dst.bottom = (src.bottom - src.top) + self->rect_b.top;
		SCREEN_DC_OPS(dc)->BitBlt(dc, v->content, &src, &dst, 0, 0);
		offset = 0;
		src.bottom = v->scroll_rect.bottom;
		dst.top = dst.bottom;
		dst.bottom = self->rect_b.bottom;
	}

	src.top = offset;
	SCREEN_DC_OPS(dc)->BitBlt(dc, v->content, &src, &dst, 0, 0);
	view_draw_children(self, dc, &self->rect_b);
}

void scroll_view_on_event(struct evt_handler *self, struct cmd_event *event)
{
	struct scroll_view *v = (struct scroll_view *) self;

	if (event->command == CMD_SCROLLVIEW_CTRL) {
		switch (event->param1) {
			case 0:
				v->interval = (int32_t) event->param2;
				EvtHandlerAddIdleHandler(self, false, v->interval);
				break;
			case 1:
				EvtHandlerRemoveIdleHandler(self, false);
				break;
			case 2:
				v->scroll_rect.right = v->scroll_rect.right - v->scroll_rect.left;
				v->scroll_rect.bottom = v->scroll_rect.bottom - v->scroll_rect.top;
				v->scroll_rect.top = 0;
				v->scroll_rect.left = v->scroll_rect.top;

				if (v->base.owner_window)
					VIEW_OPS(v)->DrawChildren(&v->base, &v->base.owner_window->windc->base, &v->base.rect_b);
				break;
		}

		event->handled = true;
	}
}

void scroll_view_on_idle_spare(struct evt_handler *self, void *task)
{
	struct scroll_view *v = (struct scroll_view *) self;
	struct rect dst;
	struct screen_dc *windc = ViewGetWinDc(&v->base);

	if (windc) {
		ScreenDcScroll(windc, &v->base.rect_b, 0, -1, 0);

		if (v->scroll_rect.bottom >= v->content_h) {
			v->scroll_rect.right = v->scroll_rect.right - v->scroll_rect.left;
			v->scroll_rect.bottom = v->scroll_rect.bottom - v->scroll_rect.top;
			v->scroll_rect.top = 0;
			v->scroll_rect.left = v->scroll_rect.top;

		} else {
			v->scroll_rect.top = v->scroll_rect.top + 1;
			v->scroll_rect.bottom = v->scroll_rect.bottom + 1;
		}

		dst = v->base.rect_b;
		dst.top = dst.bottom - 1;
		SCREEN_DC_OPS(windc)->BitBlt(windc, v->content, &v->scroll_rect, &dst, 0, 0);
	}
}
