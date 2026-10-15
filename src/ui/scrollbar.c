#include "scrollbar.h"

#include <stddef.h>

#include "../draw/drawable.h"
#include "../base/memory.h"
#include "../draw/pictserver.h"
#include "../draw/region.h"
#include "../draw/screendc.h"
#include "../draw/windc.h"
#include "window.h"

const struct scroll_bar_ops SCROLL_BAR_OPS_TABLE = {
	{
		{
			{
				{scroll_bar_destroy},
				evt_handler_is_handled,
				control_set_handled_for,
				control_set_handled,
				control_clear_handled_for,
				control_clear_handled,
				scroll_bar_on_event,
				scroll_bar_on_idle,
				view_on_idle_spare,
			},
			view_add_child,
			view_get_bounds,
			view_get_clip_region,
			control_hide,
			view_remove_child,
			view_remove_from_parent,
			view_set_cursor_id,
			control_set_device,
			control_show,
			scroll_bar_draw_children,
			view_move_children,
		},
		control_set_value,
		control_on_command,
		control_focus_sibling,
		control_on_key_down,
		control_set_focus,
		control_create_or_destroy,
	},
};

static void layout(struct scroll_bar *self, int32_t dw, int32_t dh)
{
	struct view *v = &self->base.base;

	v->rect_a.right = v->rect_a.left + dw;
	v->rect_a.bottom = v->rect_a.top + dh;
	v->rect_b.right = v->rect_b.left + dw;
	v->rect_b.bottom = v->rect_b.top + dh;
}

static void make_region(struct scroll_bar *self)
{
	struct view *v = &self->base.base;

	ObjectDelete(v->region_bounds);
	v->region_bounds = RegionCreateRect(&v->rect_b, 1, 0, 0);
}

void ScrollBarInit(struct scroll_bar *self, int32_t id, const struct event *event, const struct rect *rect,
	int32_t total, int32_t page_step, int32_t type_id)
{
	ControlInit(&self->base, CTRL_SCROLLBAR, id, event, rect, 3, -1);
	self->base.base.base.base.ops = &SCROLL_BAR_OPS_TABLE.base.base.base.base;
	self->page_step = page_step;

	struct point arrow_size = {0, 0};
	int32_t commands[3] = {CMD_MOUSE, 30004, 30005};

	self->base.slider_max = total - self->page_step;

	if (self->base.slider_max < 0)
		self->base.slider_max = 0;

	self->current_part = 0;
	self->pressed_part = 0;
	self->drawable = ServerCreateVirtual(&g_pict_server->base.base, type_id, true);

	if (self->drawable)
		DRAWABLE_OPS(self->drawable)->GetSizeAt(self->drawable, &arrow_size, 0);

	int32_t height = rect->right - rect->left;
	int32_t width = rect->bottom - rect->top;

	if (width > height) {
		layout(self, arrow_size.h, width);
		make_region(self);
		self->thumb_size = arrow_size.v;
		self->is_vertical = false;
		self->track_min = rect->top;
		self->track_max = rect->bottom;

	} else {
		layout(self, height, arrow_size.v);
		make_region(self);
		self->thumb_size = arrow_size.h;
		self->is_vertical = true;
		self->track_min = rect->left;
		self->track_max = rect->right;
	}

	self->track_min = self->track_min + self->thumb_size;
	self->track_max = self->track_max - 2 * self->thumb_size;
	self->thumb_base = self->track_min;
	self->thumb_pos = -1;
	EvtHandlerBindHandler(&self->base.base.base, 3, commands);
}

struct scroll_bar *ScrollBarCreate(int32_t id, const struct event *event, const struct rect *rect, int32_t total,
	int32_t page_step, int32_t type_id)
{
	struct scroll_bar *self = MemAlloc(1, sizeof(struct scroll_bar));
	ScrollBarInit(self, id, event, rect, total, page_step, type_id);

	return self;
}

void ScrollBarDestroy(struct scroll_bar **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void scroll_bar_destroy(struct object *self)
{
	struct scroll_bar *sb = (struct scroll_bar *) self;

	EvtHandlerRemoveIdleHandler(&sb->base.base.base, true);

	if (sb->drawable)
		ReleaseDrawable(&sb->drawable->base.base, false);

	self->ops = &CONTROL_OPS_TABLE.base.base.base;
	control_destroy(self);
}

void scroll_bar_draw_children(struct view *self, struct screen_dc *dc, const struct rect *invalid_rect)
{
	struct scroll_bar *sb = (struct scroll_bar *) self;
	bool is_handled = evt_handler_is_handled(&self->base);
	struct rect rect = VIEW_OPS(self)->GetBounds(self);

	ScreenDcSetPenWidth2(dc, 1);
	SCREEN_DC_OPS(dc)->SetColor(dc, 0);

	if (is_handled) {
		SCREEN_DC_OPS(dc)->SetBkColor(dc, 2);
		ScreenDcFillRect(dc, &rect, 0);
		ScreenDcFrameRect(dc, &rect, 0);
		ScrollBarDrawPart(sb, dc, 1, sb->current_part == 1);
		ScrollBarDrawPart(sb, dc, 5, sb->current_part == 5);
		ScrollBarDrawPart(sb, dc, 3, false);

		if (sb->thumb_pos > 0)
			ScrollBarDrawPart(sb, dc, 6, false);

	} else {
		SCREEN_DC_OPS(dc)->SetBkColor(dc, 236);
		ScreenDcFillRect(dc, &rect, 0);
		ScreenDcFrameRect(dc, &rect, 0);
		ScrollBarDrawPart(sb, dc, 1, false);
		ScrollBarDrawPart(sb, dc, 5, false);
	}

	SCREEN_DC_OPS(dc)->SetColor(dc, 0);
	SCREEN_DC_OPS(dc)->SetBkColor(dc, 255);
}

static void fire_action(struct scroll_bar *self)
{
	struct cmd_event *cmd = CmdEventCreateSpecsFlag(CMD_CONTROL_ACTION, &self->base.base.base.eh_event,
		&self->base.c_event, false);
	DispatchGlobalEvent(cmd);
}

void scroll_bar_on_event(struct evt_handler *self, struct cmd_event *ev)
{
	struct scroll_bar *sb = (struct scroll_bar *) self;
	struct control *ctrl = &sb->base;
	struct window *owner = ctrl->base.owner_window;
	struct point cursor = {0, 0};

	if (!owner) {
		ev->handled = true;
		return;
	}

	struct screen_dc *dc = &owner->windc->base;

	switch (ev->command) {
		case CMD_MOUSE:
			if (((struct mouse_event *) ev)->button_down) {
				cursor = WindowCursorToClient(owner);

				if (ViewPointInBounds(&ctrl->base, &cursor)) {
					ev->handled = true;
					ctrl->slider_value = 1;
					sb->pressed_part = ScrollBarHitTest(sb, &cursor, 0);
					sb->current_part = sb->pressed_part;
					sb->thumb_pos = sb->thumb_base;

					if (sb->current_part == 3) {
						sb->drag_offset = (sb->is_vertical ? cursor.h : cursor.v) - sb->thumb_base;
						ScrollBarDrawPart(sb, dc, 6, false);

					} else {
						ScrollBarDrawPart(sb, dc, sb->current_part, true);
					}

					EvtHandlerAddIdleHandler(self, true, 1);
				}

			} else {
				if (EvtHandlerFindIdleHandler(self, true)) {
					ev->handled = true;
					EvtHandlerRemoveIdleHandler(self, true);
					sb->pressed_part = ScrollBarHitTest(sb, &cursor, sb->thumb_size);
					int32_t old_value = ctrl->value;

					if (sb->current_part == 3) {
						ScrollBarDrawPart(sb, dc, 6, true);
						ScrollBarDrawPart(sb, dc, 3, true);
						ScrollBarValueForPart(sb, sb->current_part, &ctrl->value);
						ScrollBarDrawPart(sb, dc, 3, false);

					} else {
						ScrollBarDrawPart(sb, dc, sb->current_part, false);
						old_value = -1;
					}

					ctrl->slider_value = 0;
					sb->pressed_part = 0;
					sb->current_part = sb->pressed_part;
					sb->thumb_pos = -1;

					if (ctrl->value != old_value)
						fire_action(sb);
				}
			}
			break;
	}
}

void scroll_bar_on_idle(struct evt_handler *self, void *task)
{
	struct scroll_bar *sb = (struct scroll_bar *) self;
	struct control *ctrl = &sb->base;
	struct window *owner = ctrl->base.owner_window;
	int32_t coord;
	int32_t new_value;

	if (!owner)
		return;

	struct screen_dc *dc = &owner->windc->base;
	struct point cursor = WindowCursorToClient(owner);

	if (sb->is_vertical) {
		coord = cursor.h;

	} else {
		coord = cursor.v;
	}

	int32_t hit_part = ScrollBarHitTest(sb, &cursor, sb->thumb_size);

	if (sb->current_part == 3) {
		if (!hit_part) {
			ScrollBarDrawPart(sb, dc, 6, true);

		} else {
			new_value = coord - sb->drag_offset;

			if (sb->track_min > new_value) {
				new_value = sb->track_min;

			} else if (sb->track_max < new_value) {
				new_value = sb->track_max;
			}

			if (sb->thumb_pos != new_value) {
				if (sb->pressed_part != 0)
					ScrollBarDrawPart(sb, dc, 6, true);
			}

			if (sb->thumb_pos != new_value || sb->pressed_part == 0) {
				sb->thumb_pos = new_value;
				ScrollBarDrawPart(sb, dc, 6, false);
			}
		}

	} else {
		if (sb->current_part == hit_part && sb->current_part != 0) {
			new_value = ctrl->value;

			if (ScrollBarValueForPart(sb, sb->current_part, &new_value)) {
				ScrollBarDrawPart(sb, dc, 3, true);
				ctrl->value = new_value;
				ScrollBarDrawPart(sb, dc, 3, false);
				fire_action(sb);
			}
		}

		if (sb->pressed_part != hit_part)
			ScrollBarDrawPart(sb, dc, sb->current_part, sb->current_part == hit_part);
	}

	sb->pressed_part = hit_part;
}

void ScrollBarSetMax(struct scroll_bar *self, int32_t new_max)
{
	struct control *ctrl = &self->base;
	struct evt_handler *eh = &ctrl->base.base;
	int32_t old_max = ctrl->slider_max;
	ctrl->slider_max = new_max;

	if (ctrl->slider_min > ctrl->slider_max)
		ctrl->slider_min = ctrl->slider_max;

	if (EVT_HANDLER_OPS(eh)->IsHandled(eh)) {
		if (ctrl->slider_max == 0)
			EVT_HANDLER_OPS(eh)->ClearHandled(eh);

	} else {
		if (ctrl->slider_max != 0)
			EVT_HANDLER_OPS(eh)->SetHandled(eh);
	}

	if (ctrl->slider_max != old_max) {
		if (ctrl->base.owner_window) {
			struct rect bounds = VIEW_OPS(self)->GetBounds(&ctrl->base);
			VIEW_OPS(self)->DrawChildren(&ctrl->base, &ctrl->base.owner_window->windc->base, &bounds);
		}
	}
}

int32_t ScrollBarHitTest(struct scroll_bar *self, const struct point *pt, int32_t h)
{
	int32_t cross, pos, pos_lo, pos_hi, cross_lo, cross_hi;
	struct rect bounds = view_get_bounds(&self->base.base);

	if (self->is_vertical) {
		cross = pt->v;
		pos = pt->h;
		pos_lo = bounds.left;
		pos_hi = bounds.right;
		cross_lo = bounds.top - h;
		cross_hi = bounds.bottom + h;

	} else {
		cross = pt->h;
		pos = pt->v;
		pos_lo = bounds.top;
		pos_hi = bounds.bottom;
		cross_lo = bounds.left - h;
		cross_hi = bounds.right + h;
	}

	if (pos < pos_lo || pos > pos_hi || cross < cross_lo || cross > cross_hi)
		return 0;

	if (self->track_min > pos)
		return 1;

	if (self->thumb_base > pos)
		return 2;

	if (self->thumb_size + self->thumb_base > pos)
		return 3;

	if (self->thumb_size + self->track_max > pos)
		return 4;

	return 5;
}

bool ScrollBarValueForPart(struct scroll_bar *self, int32_t part, int32_t *out)
{
	struct control *ctrl = &self->base;
	int32_t new_value = ctrl->value;
	int32_t old_value = new_value;

	switch (part) {
		case 1:
			--new_value;
			break;
		case 5:
			++new_value;
			break;
		case 2:
			new_value -= self->page_step;
			break;
		case 4:
			new_value += self->page_step;
			break;
		case 3:
			new_value = (self->thumb_pos - self->track_min + self->thumb_size / 2) * ctrl->slider_max /
				(self->track_max - self->track_min);
			break;
	}

	if (ctrl->slider_min > new_value) {
		new_value = ctrl->slider_min;

	} else {
		if (ctrl->slider_max < new_value)
			new_value = ctrl->slider_max;
	}

	*out = new_value;

	return old_value != new_value;
}

void ScrollBarDrawPart(struct scroll_bar *self, struct screen_dc *dc, int32_t part, bool pressed)
{
	struct control *ctrl = &self->base;
	struct point origin = {0, 0};
	struct rect thumb_rect = {0, 0, 0, 0};
	struct rect base_rect = {0, 0, 0, 0};
	bool is_handled = evt_handler_is_handled(&ctrl->base.base);
	struct rect bounds = VIEW_OPS(self)->GetBounds(&ctrl->base);
	int32_t vertical = self->is_vertical;

	switch (part) {
		case 1:
			origin = PointMake(bounds.left, bounds.top);

			if (self->drawable)
				DRAWABLE_OPS(self->drawable)->Draw(self->drawable, dc, &origin,
					is_handled ? pressed + 2 * vertical : 2 * vertical + 12, 0);
			break;
		case 5:
			if (!self->is_vertical) {
				origin = PointMake(bounds.left, bounds.bottom - self->thumb_size);

			} else {
				origin = PointMake(bounds.right - self->thumb_size, bounds.top);
			}

			if (self->drawable)
				DRAWABLE_OPS(self->drawable)->Draw(self->drawable, dc, &origin,
					is_handled ? pressed + 2 * vertical + 4 : 2 * vertical + 13, 0);
			break;
		case 6:
			if (self->thumb_base != self->thumb_pos && is_handled) {
				if (pressed) {
					SCREEN_DC_OPS(dc)->SetColor(dc, 2);

				} else {
					SCREEN_DC_OPS(dc)->SetColor(dc, 0);
				}

				if (!self->is_vertical) {
					base_rect = RectMake(self->thumb_base, bounds.left + 1, self->thumb_base + self->thumb_size,
						bounds.right - 1);
					thumb_rect = RectMake(self->thumb_pos, bounds.left + 1, self->thumb_pos + self->thumb_size,
						bounds.right - 1);

				} else {
					base_rect = RectMake(bounds.top + 1, self->thumb_base, bounds.bottom - 1,
						self->thumb_base + self->thumb_size);
					thumb_rect = RectMake(bounds.top + 1, self->thumb_pos, bounds.bottom - 1,
						self->thumb_pos + self->thumb_size);
				}

				ScreenDcFrameRect(dc, &thumb_rect, 0);
				SCREEN_DC_OPS(dc)->SetColor(dc, 0);

				struct rect tmp;
				if (RectIntersect(&thumb_rect, &base_rect, &tmp))
					ScrollBarDrawPart(self, dc, 3, false);
			}
			break;
		case 3:
			if (!pressed && is_handled) {
				self->thumb_base = (self->track_max - self->track_min) * ctrl->value / ctrl->slider_max +
					self->track_min;

				if (!self->is_vertical) {
					origin = PointMake(bounds.left + 1, self->thumb_base);

				} else {
					origin = PointMake(self->thumb_base, bounds.top + 1);
				}

				if (self->drawable)
					DRAWABLE_OPS(self->drawable)->Draw(self->drawable, dc, &origin, 2 * vertical + 8, 0);

			} else {
				if (!self->is_vertical) {
					base_rect = RectMake(self->thumb_base, bounds.left + 1, self->thumb_base + self->thumb_size,
						bounds.right - 1);

				} else {
					base_rect = RectMake(bounds.top + 1, self->thumb_base, bounds.bottom - 1,
						self->thumb_base + self->thumb_size);
				}

				SCREEN_DC_OPS(dc)->SetBkColor(dc, 2);
				ScreenDcFillRect(dc, &base_rect, 0);
				SCREEN_DC_OPS(dc)->SetBkColor(dc, 255);
			}
			break;
	}
}
