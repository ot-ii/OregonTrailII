#include "../combobox.h"

#include <stddef.h>

#include <windows.h>

#include "../../app/app.h"
#include "../../base/cstr.h"
#include "../../draw/font.h"
#include "../../draw/fonttable.h"
#include "../../data/gfxdev.h"
#include "../../base/list.h"
#include "../../base/memory.h"
#include "../../draw/region.h"
#include "../scaler.h"
#include "../../data/strrsrc.h"
#include "../window.h"

static const int32_t g_combo_handler_table[4] = {CMD_MOUSE, CMD_KEY_DOWN, CMD_FOCUS_GAINED, CMD_FOCUS_LOST};
static WNDPROC g_combo_prevproc;

const struct combo_box_ops COMBO_BOX_OPS_TABLE = {
	{
		{
			{
				{combo_box_destroy},
				evt_handler_is_handled,
				control_set_handled_for,
				control_set_handled,
				control_clear_handled_for,
				control_clear_handled,
				combo_box_on_event,
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
			combo_box_draw_children,
			view_move_children,
		},
		combo_box_set_value,
		combo_box_on_command,
		control_focus_sibling,
		control_on_key_down,
		control_set_focus,
		combo_box_create_or_destroy,
	},
};

static LRESULT CALLBACK combo_subclass_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

void ComboBoxInit(struct combo_box *self, int32_t id, const struct event *event, const struct rect *rect,
	int32_t field94val, struct strrsrc *init_items, int32_t val)
{
	struct control *ctrl = &self->base;

	ControlInit(ctrl, CTRL_COMBOBOX, id, event, rect, 2, -1);
	ctrl->base.base.base.ops = &COMBO_BOX_OPS_TABLE.base.base.base.base;

	if (init_items) {
		self->items = init_items;

	} else {
		self->items = StrRsrcCreate();
	}

	ctrl->value = val;
	self->font_id = 0;
	self->field_88 = 255;
	self->field_8C = 0;
	self->field_90 = 0;
	self->drop_height_extra = field94val;
	ctrl->owns_window = true;
	ctrl->hwnd = NULL;
	ObjectDelete(ctrl->base.region_clip);
	ctrl->base.region_clip = RegionCreate();
	EvtHandlerBindHandler(&ctrl->base.base, 4, g_combo_handler_table);
}

struct combo_box *ComboBoxCreate(int32_t id, const struct event *event, const struct rect *rect, int32_t field94val,
	struct strrsrc *init_items, int32_t val)
{
	struct combo_box *self = MemAlloc(1, sizeof(struct combo_box));
	ComboBoxInit(self, id, event, rect, field94val, init_items, val);

	return self;
}

void ComboBoxDestroy(struct combo_box **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void combo_box_destroy(struct object *self)
{
	struct combo_box *cb = (struct combo_box *) self;
	struct control *ctrl = &cb->base;

	ControlRelinquishFocus(ctrl);
	EvtHandlerReleaseBinding(&ctrl->base.base, 4, g_combo_handler_table);

	if (ctrl->owns_window) {
		if (ctrl->base.owner_window)
			VIEW_OPS(ctrl)->SetDevice(&ctrl->base, NULL);
	}

	if (cb->items)
		StrRsrcDestroy(&cb->items);

	self->ops = &CONTROL_OPS_TABLE.base.base.base;
	control_destroy(self);
}

struct view *ComboCreateFromDesc(struct ctrl_desc *desc)
{
	struct rect rect = RectMake(desc->rect.top, desc->rect.left, desc->rect.bottom, desc->rect.right);
	struct strrsrc *options = NULL;
	int32_t rsrc_id = desc->rsrc_id;

	if (desc->src) {
		options = desc->src;

	} else if (rsrc_id > 0) {
		options = StrRsrcLoad(g_text_lib, rsrc_id, true);
	}

	struct combo_box *combo = ComboBoxCreate(desc->id, desc->event, &rect, desc->style, options, desc->value);
	ControlApplyDescriptor(&combo->base, desc);

	return &combo->base.base;
}

void combo_box_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds)
{
	struct control *ctrl = (struct control *) self;

	if (self->enabled) {
		if (ctrl->hwnd) {
			HWND hwnd = (HWND) ctrl->hwnd;
			struct rect own = VIEW_OPS(self)->GetBounds(self);
			struct rect rc;

			if (RectIntersect(bounds, &own, &rc)) {
				if (!IsWindowVisible(hwnd))
					ShowWindow(hwnd, SW_SHOWNORMAL);

				RECT window_rect;
				GetWindowRect(hwnd, &window_rect);
				ScreenToClient(hwnd, (LPPOINT) &window_rect.left);
				ScreenToClient(hwnd, (LPPOINT) &window_rect.right);
				InvalidateRect(hwnd, &window_rect, FALSE);
				UpdateWindow(hwnd);
			}
		}
	}
}

void combo_box_on_event(struct evt_handler *self, struct cmd_event *ev)
{
	struct combo_box *cb = (struct combo_box *) self;

	if (!cb->base.hwnd)
		return;

	switch (ev->command) {
		case CMD_MOUSE:
			ev->handled = true;
			break;
		case CMD_KEY_DOWN:
			ev->handled = true;
			break;
	}
}

void ComboBoxAddItem(struct combo_box *self, struct cstr *item)
{
	if (self->base.hwnd)
		SendMessageA((HWND) self->base.hwnd, CB_ADDSTRING, 0, (LPARAM) CStrGet(item));

	StrRsrcAppend(self->items, CStrGet(item));
}

void combo_box_set_value(struct control *self, int32_t value, bool notify)
{
	struct combo_box *cb = (struct combo_box *) self;

	if (self->hwnd) {
		if (value >= 0 && StrRsrcCount(cb->items) > value) {
			SendMessageA((HWND) self->hwnd, CB_SETCURSEL, (WPARAM) value, 0);

		} else {
			SendMessageA((HWND) self->hwnd, CB_SETCURSEL, (WPARAM) -1, 0);
		}
	}

	control_set_value(self, value, notify);
}

intptr_t ComboBoxHandleKeydown(struct combo_box *self, void *hwnd, uint32_t msg, uintptr_t wparam, intptr_t lparam)
{
	struct control *ctrl = &self->base;

	switch (msg) {
		case WM_KEYDOWN:
			CONTROL_OPS(ctrl)->OnKeyDown(ctrl, ctrl->hwnd, (uint32_t) wparam, 1, (int16_t) lparam, HIWORD(lparam));
			return 0;
		default:
			return CallWindowProcA((WNDPROC) ctrl->orig_wndproc, (HWND) hwnd, msg, wparam, lparam);
	}
}

void combo_box_create_or_destroy(struct control *self, struct window *parent)
{
	struct combo_box *cb = (struct combo_box *) self;
	struct view *view = &self->base;

	if (!self->owns_window)
		return;

	if (parent) {
		struct rect bounds = VIEW_OPS(view)->GetBounds(view);
		HWND hwnd = CreateWindowExA(0, "COMBOBOX", NULL, 0x40A30203, bounds.left, bounds.top,
			bounds.right - bounds.left, cb->drop_height_extra + (bounds.bottom - bounds.top), (HWND) parent->hwnd,
			NULL, (HINSTANCE) g_hinstance, NULL);
		self->hwnd = hwnd;
		SetPropA(hwnd, "CCLO", (HANDLE) self);

		struct font *font = ObarrayGetAt(g_font_table, cb->font_id);
		SendMessageA(hwnd, WM_SETFONT, (WPARAM) font->hfont, 0);
		ScalerChild(hwnd, bounds.left, bounds.top, bounds.right - bounds.left,
			cb->drop_height_extra + (bounds.bottom - bounds.top), NULL);

		for (int32_t i = 0; StrRsrcCount(cb->items) > i; i++) {
			struct cstr *item = StrRsrcGetCStr(cb->items, i);
			SendMessageA(hwnd, CB_ADDSTRING, 0, (LPARAM) CStrGet(item));
			CStrDestroy(&item);
		}

		if (self->value >= 0 && StrRsrcCount(cb->items) > self->value) {
			SendMessageA(hwnd, CB_SETCURSEL, (WPARAM) self->value, 0);

		} else {
			SendMessageA(hwnd, CB_SETCURSEL, (WPARAM) -1, 0);
		}

		self->orig_wndproc = (void *) SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR) combo_subclass_wndproc);
		g_combo_prevproc = (WNDPROC) self->orig_wndproc;

	} else if (self->hwnd) {
		HWND hwnd = (HWND) self->hwnd;
		SendMessageA(hwnd, WM_KILLFOCUS, 0, 0);
		RemovePropA(hwnd, "CCLO");
		RemovePropA(hwnd, "CCHI");
		DestroyWindow(hwnd);
		self->hwnd = NULL;
	}
}

static LRESULT CALLBACK combo_subclass_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	struct combo_box *combo = (struct combo_box *) GetPropA(hwnd, "CCLO");

	if (combo)
		return ComboBoxHandleKeydown(combo, hwnd, msg, wparam, lparam);

	return CallWindowProcA(g_combo_prevproc, hwnd, msg, wparam, lparam);
}

void ComboBoxSetList(struct combo_box *self, struct strrsrc *new_items, int32_t value)
{
	if (self->items)
		StrRsrcDestroy(&self->items);

	self->items = new_items;

	if (self->base.hwnd) {
		HWND hwnd = (HWND) self->base.hwnd;
		SendMessageA(hwnd, CB_RESETCONTENT, 0, 0);
		int32_t count = StrRsrcCount(self->items);

		if (count) {
			for (int32_t i = 0; i < count; i++)
				SendMessageA(hwnd, CB_ADDSTRING, 0, (LPARAM) StrRsrcGetAt(self->items, i));
		}
	}

	CONTROL_OPS(self)->SetValue(&self->base, value, true);
}

void combo_box_on_command(struct control *self, void *parent_hwnd, int32_t id, void *ctrl_hwnd, int32_t notify)
{
	switch (notify) {
		case CBN_SELCHANGE:
			self->value = (int32_t) SendMessageA((HWND) self->hwnd, CB_GETCURSEL, 0, 0);
			self->command_p1 = self->value;
			ControlFireCommand(self);
			break;
	}
}
