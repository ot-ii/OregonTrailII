#include "../control.h"

#include <stddef.h>

#include <windows.h>

#include "../../base/log.h"
#include "../../base/memory.h"
#include "../../draw/region.h"
#include "../../app/replay.h"
#include "../../base/str.h"
#include "../../draw/windc.h"
#include "../window.h"

static const int32_t g_ctrl_handler_table[3] = {CMD_FOCUS_GAINED, CMD_FOCUS_LOST, CMD_FOCUS_KEPT};

const struct control_ops CONTROL_OPS_TABLE = {
	{
		{
			{control_destroy},
			evt_handler_is_handled,
			control_set_handled_for,
			control_set_handled,
			control_clear_handled_for,
			control_clear_handled,
			control_on_event,
			view_on_idle,
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
		control_draw_children,
		view_move_children,
	},
	control_set_value,
	control_on_command,
	control_focus_sibling,
	control_on_key_down,
	control_set_focus,
	control_create_or_destroy,
};

void ControlInit(struct control *self, int32_t control_type, int32_t id, const struct event *event,
	const struct rect *rect, int32_t flags, int32_t focus_order)
{
	ViewInit(&self->base, EVCLS_CONTROL, id, rect);
	self->base.base.base.ops = &CONTROL_OPS_TABLE.base.base.base;
	self->c_event = *event;

	self->control_type = control_type;
	self->flags = flags;
	self->focus_order = focus_order;
	ControlInitFields(self);
	EvtHandlerBindHandler(&self->base.base, 3, g_ctrl_handler_table);
}

void ControlInitPoint(struct control *self, int32_t control_type, int32_t id, const struct event *event_src,
	const struct point *pos, int32_t flags, int32_t focus_order)
{
	ViewInitPoint(&self->base, EVCLS_CONTROL, id, pos);
	self->base.base.base.ops = &CONTROL_OPS_TABLE.base.base.base;
	self->c_event = *event_src;

	self->control_type = control_type;
	self->flags = flags;
	self->focus_order = focus_order;
	ControlInitFields(self);
	EvtHandlerBindHandler(&self->base.base, 3, g_ctrl_handler_table);
}

struct control *ControlCreate(int32_t control_type, int32_t id, const struct event *event, const struct rect *rect,
	int32_t flags, int32_t focus_order)
{
	struct control *self = MemAlloc(1, sizeof(struct control));
	ControlInit(self, control_type, id, event, rect, flags, focus_order);

	return self;
}

struct control *ControlCreatePoint(int32_t control_type, int32_t id, const struct event *event_src,
	const struct point *pos, int32_t flags, int32_t focus_order)
{
	struct control *self = MemAlloc(1, sizeof(struct control));
	ControlInitPoint(self, control_type, id, event_src, pos, flags, focus_order);

	return self;
}

void ControlDestroy(struct control **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void control_destroy(struct object *obj)
{
	struct control *self = (struct control *) obj;

	struct window *win = self->base.owner_window;
	EvtHandlerReleaseBinding(&self->base.base, 3, g_ctrl_handler_table);

	if (win)
		WindowRemoveFocusCtrl(win, self);

	obj->ops = &VIEW_OPS_TABLE.base.base;
	view_destroy(obj);
}

void control_set_handled(struct evt_handler *self)
{
	if (!EVT_HANDLER_OPS(self)->IsHandled(self)) {
		evt_handler_set_handled(self);
		ViewNotifyChanged((struct view *) self);
	}
}

void control_set_handled_for(struct evt_handler *self, struct event *event)
{
	evt_handler_set_handled_for(self, event);
}

void control_clear_handled(struct evt_handler *self)
{
	if (EVT_HANDLER_OPS(self)->IsHandled(self)) {
		evt_handler_clear_handled(self);
		ViewNotifyChanged((struct view *) self);
	}
}

void control_clear_handled_for(struct evt_handler *self, struct event *event)
{
	evt_handler_clear_handled_for(self, event);
}

void control_draw_children(struct view *view, struct screen_dc *dc, const struct rect *invalid_rect)
{
	(void) invalid_rect;
	struct control *self = (struct control *) view;

	view_draw_children(view, dc, &view->rect_b);

	if (self->redraw_pending) {
		CONTROL_OPS(self)->SetFocus(self, true);
		ViewNotifyChanged(view);
		self->redraw_pending = false;
	}
}

void control_set_focus(struct control *self, bool gaining)
{
	if (self->hwnd) {
		if (gaining)
			SetFocus((HWND) self->hwnd);
	}
}

void control_hide(struct view *view, bool fill, bool invalidate)
{
	struct control *self = (struct control *) view;

	if (self->hwnd)
		ShowWindow((HWND) self->hwnd, 0);

	view_hide(view, fill, invalidate);
}

void ControlGetDims(struct control *self, int32_t *out_id, int32_t *out_p1, int32_t *out_p2)
{
	*out_id = self->command_id;
	*out_p1 = self->command_p1;
	*out_p2 = self->command_p2;
}

void control_on_event(struct evt_handler *handler, struct cmd_event *event)
{
	struct control *self = (struct control *) handler;

	switch (event->command) {
		case CMD_FOCUS_GAINED:
			if (self->base.owner_window && self->base.owner_window->shown) {
				CONTROL_OPS(self)->SetFocus(self, true);

			} else {
				self->redraw_pending = true;
			}

			event->handled = true;
			break;

		case CMD_FOCUS_LOST:
			CONTROL_OPS(self)->SetFocus(self, false);
			event->handled = true;
			break;

		case CMD_FOCUS_KEPT:
			event->handled = true;
			break;
	}
}

void ControlFireCommand(struct control *self)
{
	struct cmd_event *cmd = CmdEventCreateSpecsFlag(self->command_id, &self->base.base.eh_event, &self->c_event,
		false);
	cmd->param1 = self->command_p1;
	cmd->param2 = self->command_p2;
	DispatchGlobalEvent(cmd);
}

void control_set_value(struct control *self, int32_t value, bool notify)
{
	self->value = value;

	if (notify && self->base.enabled) {
		if (self->base.owner_window) {
			struct rect bounds = VIEW_OPS(self)->GetBounds(&self->base);
			VIEW_OPS(self)->DrawChildren(&self->base, &self->base.owner_window->windc->base, &bounds);
		}
	}
}

void ControlSetSliderValue(struct control *self, int32_t value)
{
	int32_t old = self->slider_value;
	self->slider_value = value;

	if (self->slider_value != old) {
		if (self->base.owner_window) {
			struct rect bounds = VIEW_OPS(self)->GetBounds(&self->base);
			VIEW_OPS(self)->DrawChildren(&self->base, &self->base.owner_window->windc->base, &bounds);
		}
	}
}

void ControlSetSliderMax(struct control *self, int32_t max)
{
	int32_t old = self->slider_max;
	self->slider_max = max;

	if (self->slider_min > self->slider_max)
		self->slider_min = self->slider_max;

	if (self->slider_max != old) {
		if (self->base.owner_window) {
			struct rect bounds = VIEW_OPS(self)->GetBounds(&self->base);
			VIEW_OPS(self)->DrawChildren(&self->base, &self->base.owner_window->windc->base, &bounds);
		}
	}
}

void ControlSetSliderMin(struct control *self, int32_t min)
{
	int32_t old = self->slider_min;
	self->slider_min = min;

	if (self->slider_min > self->slider_max)
		self->slider_max = self->slider_min;

	if (self->slider_min != old) {
		if (self->base.owner_window) {
			struct rect bounds = VIEW_OPS(self)->GetBounds(&self->base);
			VIEW_OPS(self)->DrawChildren(&self->base, &self->base.owner_window->windc->base, &bounds);
		}
	}
}

void ControlSetCommand(struct control *self, int32_t id, int32_t command_p1, int32_t command_p2)
{
	self->command_id = id;
	self->command_p1 = command_p1;
	self->command_p2 = command_p2;
}

void control_create_or_destroy(struct control *self, struct window *parent)
{
	(void) self;
	(void) parent;
}

void control_set_device(struct view *view, struct window *window)
{
	struct control *self = (struct control *) view;
	char msg[100];

	struct window *win = self->base.owner_window;

	if (window && win) {
		StrPrintf(msg, "SetWindow: Window pointer already exists. CONTROLTYPE = %d", self->control_type);
		LogError(LOG_ERROR, msg);
	}

	view_set_device(view, window);
	CONTROL_OPS(self)->CreateOrDestroy(self, window);

	if (!window && win) {
		WindowRemoveFocusCtrl(win, self);

	} else if (window) {
		WindowAddFocusCtrl(window, self, self->focus_order);
	}
}

void control_show(struct view *view)
{
	struct control *self = (struct control *) view;

	if (self->hwnd)
		ShowWindow((HWND) self->hwnd, 5);

	view_show(view);
}

void control_on_command(struct control *self, void *parent_hwnd, int32_t id, void *ctrl_hwnd, int32_t notify)
{
	(void) self;
	(void) parent_hwnd;
	(void) id;
	(void) ctrl_hwnd;
	(void) notify;
}

void ControlInitFields(struct control *self)
{
	self->redraw_pending = false;
	self->wants_enter = false;
	self->owns_window = false;
	self->slider_value = 0;
	self->slider_min = 0;
	self->slider_max = 0;
	self->value = 0;
	self->is_default = false;
	self->command_id = CMD_CONTROL_ACTION;
	self->command_p1 = 0;
	self->command_p2 = 0;
	self->hwnd = NULL;
	ObjectDelete(self->base.region_clip);

	struct rect bounds = VIEW_OPS(self)->GetBounds(&self->base);
	self->base.region_clip = RegionCreateRect(&bounds, 1, 0, 0);
}

static void ct_region(struct control *self)
{
	self->is_default = true;
	ObjectDelete(self->base.region_bounds);

	struct rect bounds = VIEW_OPS(self)->GetBounds(&self->base);
	self->base.region_bounds = RegionCreateRect(&bounds, 1, 0, 0);
}

void ControlApplyDescriptor(struct control *self, struct ctrl_desc *desc)
{
	if (desc->cmd_id)
		ControlSetCommand(self, desc->cmd_id, desc->cmd_param1, desc->cmd_param2);

	if (desc->focus_order)
		ControlSetFocusOrder(self, desc->focus_order);

	if (desc->flags & 1)
		ct_region(self);

	if (desc->cursor_id > 0)
		VIEW_OPS(self)->SetCursorId(&self->base, desc->cursor_id);
}

int32_t ControlGetFocusIndex(struct control *self)
{
	return self->focus_order;
}

void ControlSetFocusOrder(struct control *self, int32_t focus_order)
{
	self->focus_order = focus_order;

	if (self->base.owner_window)
		WindowInsertCtrl(self->base.owner_window, self, focus_order);
}

void ControlRelinquishFocus(struct control *self)
{
	struct window *win = self->base.owner_window;

	if (win) {
		if (win->focus_ctrl == self) {
			self->flags = 0;
			WindowFocusNext(win);

			if (win->focus_ctrl == self)
				win->focus_ctrl = NULL;
		}
	}
}

static LPARAM make_lparam(int32_t lo, uint32_t hi)
{
	return (LPARAM) (int32_t) ((uint32_t) (uint16_t) lo | ((uint32_t) (uint16_t) hi << 16));
}

void control_on_key_down(struct control *self, void *hwnd, uint32_t wparam, int32_t reserved, int32_t lparam_lo,
	uint32_t lparam_hi)
{
	(void) reserved;

	if (wparam == 9)
		CONTROL_OPS(self)->FocusSibling(self, self->hwnd, RpGetKeyState(16) < 0);

	ControlCallDefaultWndProc(self, hwnd, 0x100, wparam, make_lparam(lparam_lo, lparam_hi));
}

void control_focus_sibling(struct control *self, void *hwnd, bool prev)
{
	(void) self;
	HWND parent = GetParent((HWND) hwnd);

	if (parent) {
		struct window *win = WindowFromHwnd(parent);

		if (!win)
			return;

		if (prev) {
			WindowFocusPrev(win);

		} else {
			WindowFocusNext(win);
		}
	}
}

intptr_t ControlCallDefaultWndProc(struct control *self, void *hwnd, uint32_t msg, uintptr_t wparam, intptr_t lparam)
{
	return CallWindowProcA((WNDPROC) self->orig_wndproc, (HWND) hwnd, msg, wparam, lparam);
}
