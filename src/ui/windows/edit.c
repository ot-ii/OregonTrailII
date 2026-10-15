#include "../edit.h"

#include <stddef.h>
#include <string.h>

#include <windows.h>

#include "../../app/app.h"
#include "../../base/cstr.h"
#include "../cursormgr.h"
#include "../../draw/font.h"
#include "../../draw/fonttable.h"
#include "../../data/gfxdev.h"
#include "../../base/list.h"
#include "../../base/memory.h"
#include "../../draw/region.h"
#include "../../app/replay.h"
#include "../scaler.h"
#include "../../draw/screendc.h"
#include "../../base/str.h"
#include "../../data/strrsrc.h"
#include "../window.h"

static const int32_t g_edit_handler_table[11] = {CMD_MOUSE, CMD_KEY_DOWN, CMD_UNDO, CMD_CUT, CMD_COPY, CMD_CLEAR,
	CMD_PASTE, CMD_FOCUS_GAINED, CMD_FOCUS_LOST, 30017, CMD_FOCUS_KEPT};

static WNDPROC g_edit_prevproc;

const struct edit_ops EDIT_OPS_TABLE = {
	{
		{
			{
				{edit_destroy},
				evt_handler_is_handled,
				control_set_handled_for,
				control_set_handled,
				control_clear_handled_for,
				control_clear_handled,
				edit_on_event,
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
			edit_draw_children,
			view_move_children,
		},
		control_set_value,
		control_on_command,
		control_focus_sibling,
		edit_on_key_down,
		edit_set_focus,
		edit_create_or_destroy,
	},
};

static LRESULT CALLBACK edit_subclass_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

void EditInit(struct edit *self, int32_t id, const struct event *event, const struct rect *rect,
	struct cstr *init_text, bool wants_enter, bool hscroll, int32_t vscroll, int32_t font_id, int32_t max_len,
	bool sel_on_focus, bool inert, int32_t input_filter, bool prefer_multiline)
{
	struct control *ctrl = &self->base;

	ControlInit(ctrl, CTRL_EDIT, id, event, rect, 3, -1);
	ctrl->base.base.base.ops = &EDIT_OPS_TABLE.base.base.base.base;

	self->hscroll = hscroll;
	self->vscroll = vscroll;
	ctrl->wants_enter = wants_enter;
	self->max_len = max_len;
	self->inert = inert;
	self->sel_on_focus = sel_on_focus;
	self->input_filter = input_filter;
	self->prefer_multiline = prefer_multiline;

	if (init_text) {
		self->text = init_text;

	} else {
		self->text = CStrCreate();
	}

	self->font_id = font_id;
	self->bk_color = 255;
	self->pen_color = 0;
	self->text_color = 0;
	ctrl->owns_window = true;
	ctrl->hwnd = NULL;
	view_set_cursor_id(&ctrl->base, CURSOR_IBEAM);
	ObjectDelete(ctrl->base.region_clip);
	ctrl->base.region_clip = RegionCreate();
	EvtHandlerBindHandler(&ctrl->base.base, 11, g_edit_handler_table);
}

struct edit *EditCreate(int32_t id, const struct event *event, const struct rect *rect, struct cstr *init_text,
	bool wants_enter, bool hscroll, int32_t vscroll, int32_t font_id, int32_t max_len, bool sel_on_focus, bool inert,
	int32_t input_filter, bool prefer_multiline)
{
	struct edit *self = MemAlloc(1, sizeof(struct edit));
	EditInit(self, id, event, rect, init_text, wants_enter, hscroll, vscroll, font_id, max_len, sel_on_focus, inert,
		input_filter, prefer_multiline);

	return self;
}

void EditDestroy(struct edit **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void edit_destroy(struct object *self)
{
	struct edit *e = (struct edit *) self;
	struct control *ctrl = &e->base;

	ControlRelinquishFocus(ctrl);
	EvtHandlerReleaseBinding(&ctrl->base.base, 11, g_edit_handler_table);

	if (e->text)
		CStrDestroy(&e->text);

	if (ctrl->hwnd)
		DestroyWindow((HWND) ctrl->hwnd);

	self->ops = &CONTROL_OPS_TABLE.base.base.base;
	control_destroy(self);
}

struct view *CtrlFactoryEdit(struct ctrl_desc *desc)
{
	struct rect rect = RectMake(desc->rect.top, desc->rect.left, desc->rect.bottom, desc->rect.right);
	struct cstr *init_text = NULL;
	int32_t string_index = desc->item_index;

	if (string_index > -1) {
		if (desc->src) {
			init_text = StrRsrcGetCStr(desc->src, string_index);

		} else if (desc->rsrc_id > 0) {
			struct strrsrc *strings = StrRsrcLoad(g_text_lib, desc->rsrc_id, true);

			if (strings)
				init_text = StrRsrcGetCStr(strings, string_index);

			StrRsrcDestroy(&strings);
		}
	}

	bool hscroll = (desc->flags & 2) != 0;
	bool wants_enter = (uint8_t) desc->margin_x != 0;
	struct edit *edit = EditCreate(desc->id, desc->event, &rect, init_text, wants_enter, hscroll, desc->margin_y,
		desc->font_id, desc->style, false, false, 0, false);
	ControlApplyDescriptor(&edit->base, desc);

	edit->pen_color = desc->pen_color;
	ViewNotifyChanged(&edit->base.base);
	edit->text_color = desc->brush_color;
	ViewNotifyChanged(&edit->base.base);
	edit->bk_color = desc->alt_brush_color;
	ViewNotifyChanged(&edit->base.base);

	return &edit->base.base;
}

void edit_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds)
{
	struct control *ctrl = (struct control *) self;

	if (self->enabled && ctrl->hwnd) {
		struct rect own = VIEW_OPS(self)->GetBounds(self);
		struct rect rc;

		if (RectIntersect(bounds, &own, &rc)) {
			InvalidateRect((HWND) ctrl->hwnd, NULL, FALSE);
			UpdateWindow((HWND) ctrl->hwnd);
		}
	}
}

static void post_committed(struct edit *self, intptr_t param1)
{
	struct cmd_event *cmd = CmdEventCreateSpecs(CMD_EDIT_COMMITTED, &self->base.base.base.eh_event,
		&self->base.c_event);
	cmd->param1 = param1;
	DispatchGlobalEvent(cmd);
}

void edit_set_focus(struct control *self, bool gaining)
{
	struct edit *e = (struct edit *) self;

	if (!e->inert) {
		if (e->sel_on_focus && gaining)
			EditSetSel(e, 0, 0x7D00);

		control_set_focus(self, gaining);

		if (!gaining) {
			if (e->change_pending) {
				e->change_pending = false;
				post_committed(e, 0);
			}
		}
	}
}

void edit_on_event(struct evt_handler *self, struct cmd_event *ev)
{
	struct edit *e = (struct edit *) self;
	struct control *ctrl = &e->base;
	HWND hwnd;

	if (ev->command != CMD_FOCUS_KEPT)
		control_on_event(self, ev);

	if (ev->handled)
		return;

	if (!ctrl->hwnd) {
		ev->handled = true;
		return;
	}

	hwnd = (HWND) ctrl->hwnd;

	if (!e->inert || ev->command == 30017) {
		switch (ev->command) {
			case CMD_MOUSE:
				ev->handled = true;
				break;
			case CMD_KEY_DOWN: {
				signed char key = (signed char) ev->param1;

				if ((key == 13 && !ctrl->wants_enter) || key == 9)
					post_committed(e, key);

				ev->handled = true;
				break;
			}
			case CMD_UNDO:
				SendMessageA(hwnd, WM_UNDO, 0, 0);
				ev->handled = true;
				break;
			case CMD_CUT:
				SendMessageA(hwnd, WM_CUT, 0, 0);
				ev->handled = true;
				break;
			case CMD_COPY:
				SendMessageA(hwnd, WM_COPY, 0, 0);
				ev->handled = true;
				break;
			case CMD_PASTE:
				if (EditValidateInput(e, 0x16, true, e->input_filter))
					SendMessageA(hwnd, WM_PASTE, 0, 0);

				ev->handled = true;
				break;
			case CMD_SELECT_ALL:
				SendMessageA(hwnd, EM_SETSEL, 0, 0x7FFF);
				ev->handled = true;
				break;
			case CMD_CLEAR:
				SendMessageA(hwnd, EM_SETSEL, 0, 0x7FFF);
				SendMessageA(hwnd, WM_CLEAR, 0, 0);
				ev->handled = true;
				break;
			case CMD_FOCUS_KEPT:
				if (e->change_pending)
					post_committed(e, 0);
				break;
			case 30017:
				ev->handled = true;
				break;
		}
	}
}

void EditSetSel(struct edit *self, uint32_t start, uint32_t end)
{
	struct screen_dc *dc = ViewGetWinDc(&self->base.base);

	if (!dc)
		return;

	SCREEN_DC_OPS(dc)->SetBkColor(dc, self->bk_color);
	SCREEN_DC_OPS(dc)->SetColor(dc, self->text_color);

	if (self->base.hwnd)
		SendMessageA((HWND) self->base.hwnd, EM_SETSEL, 0, (LPARAM) (int32_t) (((uint32_t) (uint16_t) start << 16) |
			(uint16_t) end));
}

void EditSetSelOnFocus(struct edit *self, bool val)
{
	self->sel_on_focus = val;
}

void EditSetText(struct edit *self, struct cstr *text)
{
	if (self->base.hwnd)
		SetWindowTextA((HWND) self->base.hwnd, CStrGet(text));

	if (self->text)
		CStrDestroy(&self->text);

	self->text = text;
}

static struct cstr *window_text(HWND hwnd, int32_t len)
{
	char *spaces = MemAlloc((size_t) len + 1, 1);
	memset(spaces, ' ', (size_t) len);
	struct cstr *result = CStrCreateBytes(len, spaces);
	MemFree(spaces);

	GetWindowTextA(hwnd, CStrGetMutable(result), len + 1);

	return result;
}

struct cstr *EditGetText(struct edit *self)
{
	struct cstr *result;

	if (self->base.hwnd) {
		int32_t len = GetWindowTextLengthA((HWND) self->base.hwnd);

		if (len) {
			result = window_text((HWND) self->base.hwnd, len);

		} else {
			result = CStrCreate();
		}

	} else {
		result = CStrCreateCopy(self->text);
	}

	return result;
}

static int32_t bounds_height(struct view *view)
{
	struct rect b = VIEW_OPS(view)->GetBounds(view);

	return b.bottom - b.top;
}

void edit_create_or_destroy(struct control *self, struct window *parent)
{
	struct edit *e = (struct edit *) self;
	struct view *view = &self->base;

	if (!self->owns_window)
		return;

	if (parent) {
		struct screen_dc *dc = ViewGetWinDc(view);

		if (!dc)
			return;

		struct rect bounds = VIEW_OPS(view)->GetBounds(view);
		int32_t metrics[4];
		ScreenDcSelectFont(dc, e->font_id);
		ScreenDcGetTextMetrics(dc, metrics, e->font_id);

		DWORD style = 0;

		if (e->prefer_multiline && bounds_height(view) < 2 * metrics[3]) {
			style = 0x80;

		} else if (bounds_height(view) > 2 * metrics[3]) {
			style = 4;
		}

		if (e->hscroll)
			style |= 0x800000;

		if (e->vscroll)
			style |= 0x200000;

		HWND hwnd = CreateWindowExA(0, "EDIT", NULL, style | 0x50000000, bounds.left, bounds.top,
			bounds.right - bounds.left, bounds.bottom - bounds.top, (HWND) parent->hwnd, NULL,
			(HINSTANCE) g_hinstance, NULL);
		self->hwnd = hwnd;

		struct obarray *fonts = g_font_table;
		int32_t count = ObarrayCount(fonts);
		int32_t font_index;

		for (font_index = 1; count > font_index
			&& ((struct font *) ObarrayGetAt(fonts, font_index))->font_id != e->font_id; font_index++)
			;

		if (count > font_index)
			SendMessageA(hwnd, WM_SETFONT, (WPARAM) ((struct font *) ObarrayGetAt(fonts, font_index))->hfont, 0);

		ScalerChild(hwnd, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top, NULL);
		SetPropA(hwnd, "CCLO", (HANDLE) self);
		self->orig_wndproc = (void *) SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR) edit_subclass_wndproc);
		g_edit_prevproc = (WNDPROC) self->orig_wndproc;
		SetWindowTextA(hwnd, CStrGet(e->text));

	} else {
		HWND hwnd = (HWND) self->hwnd;
		int32_t len = GetWindowTextLengthA(hwnd);

		if (e->text)
			CStrDestroy(&e->text);

		if (len > 0) {
			e->text = window_text(hwnd, len);

		} else {
			e->text = CStrCreate();
		}

		if (hwnd) {
			RemovePropA(hwnd, "CCLO");
			RemovePropA(hwnd, "CCHI");
			DestroyWindow(hwnd);
			self->hwnd = NULL;
		}
	}
}

void edit_on_key_down(struct control *self, void *hwnd, uint32_t wparam, int32_t reserved, int32_t lparam_lo,
	uint32_t lparam_hi)
{
	struct edit *e = (struct edit *) self;

	if (wparam == 9)
		CONTROL_OPS(self)->FocusSibling(self, self->hwnd, RpGetKeyState(VK_SHIFT) < 0);

	if (wparam == 0x2D && RpGetKeyState(VK_SHIFT) < 0 && !EditValidateInput(e, 0x16, true, e->input_filter))
		return;

	if (wparam == 0xD) {
		HWND parent = GetParent((HWND) self->hwnd);

		if (parent) {
			struct window *win = WindowFromHwnd(parent);

			if (!win)
				return;

			struct obarray *children = ViewGetChildList(win->root_view);

			if (children) {
				int32_t count = ObarrayCount(children);

				for (int32_t i = 0; i < count; i++) {
					struct view *item = ObarrayGetAt(children, i);
					int32_t code = item->base.eh_event.code;

					if (code == EVCLS_CONTROL || (code >= 0 && code <= 9)) {
						struct control *ctl = (struct control *) item;

						if (ctl->base.enabled && ctl->base.base.handled && ctl->is_default) {
							ControlFireCommand(ctl);
							return;
						}
					}
				}
			}
		}
	}

	ControlCallDefaultWndProc(self, hwnd, WM_KEYDOWN, wparam,
		(intptr_t) (int32_t) (((uint32_t) (uint16_t) lparam_hi << 16) | (uint16_t) lparam_lo));
}

intptr_t EditDispatchMessage(struct edit *self, void *hwnd, uint32_t msg, uintptr_t wparam, intptr_t lparam)
{
	struct control *ctrl = &self->base;

	switch (msg) {
		case WM_CHAR:
			EditOnChar(self, ctrl->hwnd, (uint32_t) (int32_t) (signed char) wparam, (int16_t) lparam);
			return 0;
		case WM_KILLFOCUS:
			EditOnKillFocus(self, ctrl->hwnd, wparam);
			return 0;
		case WM_LBUTTONDOWN:
			EditOnButton(self, ctrl->hwnd, 0, (int16_t) lparam, (int16_t) HIWORD(lparam), wparam);
			return 0;
		case WM_KEYDOWN:
			CONTROL_OPS(ctrl)->OnKeyDown(ctrl, ctrl->hwnd, (uint32_t) wparam, 1, (int16_t) lparam, HIWORD(lparam));
			return 0;
		default:
			return CallWindowProcA((WNDPROC) ctrl->orig_wndproc, (HWND) hwnd, msg, wparam, lparam);
	}
}

int32_t EditOnButton(struct edit *self, void *hwnd, int32_t dblclk, int32_t x, int32_t y, uintptr_t wparam)
{
	struct window *owner = self->base.base.owner_window;

	if (owner)
		owner->focus_ctrl = &self->base;

	return (int32_t) ControlCallDefaultWndProc(&self->base, hwnd, ((uint32_t) dblclk < 1) ? WM_LBUTTONDOWN :
		WM_LBUTTONDBLCLK, wparam, (intptr_t) (int32_t) (((uint32_t) (uint16_t) y << 16) | ((uint32_t) x & 0xFFFF)));
}

void EditOnChar(struct edit *self, void *hwnd, uint32_t c, int32_t x)
{
	struct control *ctrl = &self->base;

	if (c == 9 || (c == 13 && !(GetWindowLongA((HWND) hwnd, GWL_STYLE) & ES_MULTILINE)))
		return;

	bool is_paste = false;

	if (c == 0x16)
		is_paste = true;

	int32_t len = GetWindowTextLengthA((HWND) hwnd) + 1;

	{
		DWORD sel_start = 0, sel_end = 0;

		ControlCallDefaultWndProc(ctrl, hwnd, EM_GETSEL, (uintptr_t) &sel_start, (intptr_t) &sel_end);

		if (sel_end > sel_start)
			len -= (int32_t) (sel_end - sel_start);
	}

	if (self->max_len && self->max_len < len && c != 8)
		return;

	if (c == 0x16 || c >= 0x20) {
		if (!EditValidateInput(self, c, is_paste, self->input_filter))
			return;
	}

	self->change_pending = true;
	ControlCallDefaultWndProc(ctrl, hwnd, WM_CHAR, (uintptr_t) (signed char) c, x & 0xFFFF);
}

int32_t EditOnKillFocus(struct edit *self, void *hwnd, uintptr_t wparam)
{
	if (self->change_pending)
		post_committed(self, 0);

	return (int32_t) ControlCallDefaultWndProc(&self->base, hwnd, WM_KILLFOCUS, wparam, 0);
}

static LRESULT CALLBACK edit_subclass_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	struct edit *edit = (struct edit *) GetPropA(hwnd, "CCLO");

	if (edit)
		return EditDispatchMessage(edit, hwnd, msg, wparam, lparam);

	return CallWindowProcA(g_edit_prevproc, hwnd, msg, wparam, lparam);
}

static bool char_rejected(struct edit *self, uint16_t c, uint16_t filter)
{
	if (c >= 0x20 && c != 0x7F) {
		if (EditIsDigit(self, c) && (filter & 2))
			return true;

		if (!EditIsDigit(self, c) && filter == 0xFD)
			return true;
	}

	return false;
}

bool EditValidateInput(struct edit *self, uint32_t c, bool is_paste, int32_t filter)
{
	uint16_t ch = (uint16_t) c;
	uint16_t filt = (uint16_t) filter;
	bool reject = false;

	if (is_paste) {
		HGLOBAL copy_handle;

		if (OpenClipboard((HWND) self->base.hwnd)) {
			HANDLE clip_handle = GetClipboardData(CF_TEXT);
			copy_handle = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, GlobalSize(clip_handle));
			char *clip_copy = GlobalLock(copy_handle);
			char *clip_data = GlobalLock(clip_handle);
			StrCopyN(clip_copy, GlobalSize(copy_handle), clip_data);

			if (clip_copy) {
				for (ch = (uint16_t) *clip_copy; ch; clip_copy++) {
					if (char_rejected(self, ch, filt)) {
						reject = true;
						break;
					}

					ch = (uint16_t) *clip_copy;
				}
			}

			GlobalUnlock(copy_handle);
			GlobalUnlock(clip_handle);
			CloseClipboard();

		} else {
			return true;
		}

		GlobalFree(copy_handle);

		if (reject)
			return false;

	} else {
		if (char_rejected(self, ch, filt))
			return false;
	}

	return true;
}

bool EditIsDigit(struct edit *self, uint32_t c)
{
	return c >= '0' && c <= '9';
}
