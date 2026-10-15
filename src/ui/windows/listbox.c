#include "../listbox.h"

#include <stddef.h>

#include <windows.h>
#include <windowsx.h>

#include "../../app/app.h"
#include "../../base/cstr.h"
#include "../../data/gfxdev.h"
#include "../../base/memory.h"
#include "../scaler.h"
#include "../../data/strrsrc.h"
#include "../window.h"

static const int32_t g_listbox_handler_table[2] = {CMD_MOUSE, CMD_KEY_DOWN};

const struct list_box_ops LIST_BOX_OPS_TABLE = {
	{
		{
			{
				{list_box_destroy},
				evt_handler_is_handled,
				control_set_handled_for,
				control_set_handled,
				control_clear_handled_for,
				control_clear_handled,
				list_box_on_event,
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
			list_box_draw_children,
			view_move_children,
		},
		control_set_value,
		list_box_on_command,
		control_focus_sibling,
		control_on_key_down,
		control_set_focus,
		list_box_create_or_destroy,
	},
};

void ListBoxInit(struct list_box *self, int32_t id, const struct event *event, const struct rect *rect,
	struct strrsrc *init_items, int32_t sel)
{
	struct control *ctrl = &self->base;

	ControlInit(ctrl, CTRL_LISTBOX, id, event, rect, 3, -1);
	ctrl->base.base.base.ops = &LIST_BOX_OPS_TABLE.base.base.base.base;

	if (init_items) {
		self->items = init_items;

	} else {
		self->items = StrRsrcCreate();
	}

	self->sel_index = sel;
	self->unused_8A = 0;
	self->bk_color = 255;
	self->pen_color = 0;
	self->text_color = 0;
	ctrl->owns_window = true;
	ctrl->hwnd = NULL;
	EvtHandlerBindHandler(&ctrl->base.base, 2, g_listbox_handler_table);
}

struct list_box *ListBoxCreate(int32_t id, const struct event *event, const struct rect *rect,
	struct strrsrc *init_items, int32_t sel)
{
	struct list_box *self = MemAlloc(1, sizeof(struct list_box));
	ListBoxInit(self, id, event, rect, init_items, sel);

	return self;
}

void ListBoxDestroy(struct list_box **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void list_box_destroy(struct object *self)
{
	struct list_box *lb = (struct list_box *) self;

	if (lb->items)
		StrRsrcDestroy(&lb->items);

	self->ops = &CONTROL_OPS_TABLE.base.base.base;
	control_destroy(self);
}

struct view *CtrlFactoryListBox(struct ctrl_desc *desc)
{
	struct rect rect = RectMake(desc->rect.top, desc->rect.left, desc->rect.bottom, desc->rect.right);
	struct strrsrc *items = NULL;

	if (desc->rsrc_id > 0)
		items = StrRsrcLoad(g_text_lib, desc->rsrc_id, true);

	struct list_box *listbox = ListBoxCreate(desc->id, desc->event, &rect, items, items == NULL ? -1 : desc->value);
	ControlApplyDescriptor(&listbox->base, desc);

	if (desc->rect.left != -1) {
		listbox->pen_color = desc->pen_color;
		ViewNotifyChanged(&listbox->base.base);
	}

	if (desc->parent_id != -1) {
		listbox->text_color = desc->brush_color;
		ViewNotifyChanged(&listbox->base.base);
	}

	if (desc->rect.top != -1) {
		listbox->bk_color = desc->alt_brush_color;
		ViewNotifyChanged(&listbox->base.base);
	}

	return &listbox->base.base;
}

void list_box_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds)
{
	(void) self;
	(void) dc;
	(void) bounds;
}

void list_box_on_event(struct evt_handler *self, struct cmd_event *ev)
{
	struct list_box *lb = (struct list_box *) self;

	if (!lb->base.hwnd)
		return;

	bool handled = true;

	switch (ev->command) {
		case CMD_MOUSE:
			break;
		case CMD_KEY_DOWN:
			break;
		default:
			handled = false;
			break;
	}

	if (handled)
		ev->handled = true;
}

void ListBoxAddItem(struct list_box *self, struct cstr *item)
{
	if (self->base.hwnd)
		ListBox_AddString((HWND) self->base.hwnd, CStrGet(item));

	StrRsrcAppend(self->items, CStrGet(item));
}

void ListBoxSetSourceById(struct list_box *self, int32_t id)
{
	if (self->items)
		StrRsrcDestroy(&self->items);

	self->items = StrRsrcLoad(g_text_lib, id, true);

	if (!self->items)
		self->items = StrRsrcCreate();

	struct window *win = self->base.base.owner_window;
	CONTROL_OPS(self)->CreateOrDestroy(&self->base, NULL);
	CONTROL_OPS(self)->CreateOrDestroy(&self->base, win);
}

void ListBoxSetSource(struct list_box *self, struct strrsrc *new_items)
{
	if (self->items)
		StrRsrcDestroy(&self->items);

	self->items = new_items;
	struct window *win = self->base.base.owner_window;
	CONTROL_OPS(self)->CreateOrDestroy(&self->base, NULL);
	CONTROL_OPS(self)->CreateOrDestroy(&self->base, win);
}

void ListBoxReset(struct list_box *self)
{
	if (self->base.hwnd)
		SendMessageA((HWND) self->base.hwnd, LB_RESETCONTENT, 0, 0);

	self->sel_index = -1;
}

void ListBoxSetSel(struct list_box *self, int32_t index)
{
	self->sel_index = index;

	if (self->base.hwnd) {
		if (index >= 0 && index < StrRsrcCount(self->items)) {
			ListBox_SetSel((HWND) self->base.hwnd, TRUE, index);

		} else {
			ListBox_SetSel((HWND) self->base.hwnd, FALSE, -1);
		}
	}
}

void list_box_create_or_destroy(struct control *self, struct window *parent)
{
	struct list_box *lb = (struct list_box *) self;
	struct rect bounds = VIEW_OPS(self)->GetBounds(&self->base);

	if (!self->owns_window)
		return;

	if (parent) {
		HWND hwnd = CreateWindowExA(0, "LISTBOX", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | 0x40L | 0x1L,
			bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top, (HWND) parent->hwnd,
			NULL, (HINSTANCE) g_hinstance, NULL);
		self->hwnd = hwnd;
		ScalerChild(hwnd, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top, NULL);
		SetPropA(hwnd, "CCLO", (HANDLE) self);

		int32_t count = StrRsrcCount(lb->items);

		if (count) {
			for (int32_t i = 0; StrRsrcCount(lb->items) > i; i++)
				ListBox_AddString(hwnd, StrRsrcGetAt(lb->items, i));
		}

		if (lb->sel_index >= 0 && lb->sel_index < StrRsrcCount(lb->items)) {
			ListBox_SetSel(hwnd, TRUE, lb->sel_index);

		} else {
			ListBox_SetSel(hwnd, FALSE, -1);
		}

	} else if (self->hwnd) {
		HWND hwnd = (HWND) self->hwnd;
		RemovePropA(hwnd, "CCLO");
		RemovePropA(hwnd, "CCHI");
		DestroyWindow(hwnd);
		self->hwnd = NULL;
	}
}

void list_box_on_command(struct control *self, void *parent_hwnd, int32_t id, void *ctrl_hwnd, int32_t notify)
{
	struct list_box *lb = (struct list_box *) self;

	switch (notify) {
		case LBN_SELCHANGE: {
			int sel = -1;
			ListBox_GetSelItems((HWND) self->hwnd, 1, &sel);
			lb->sel_index = sel;
			ControlFireCommand(self);
			break;
		}
	}
}
