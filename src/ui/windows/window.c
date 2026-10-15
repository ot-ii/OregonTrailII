#include "../window.h"

#include <stddef.h>

#include <windows.h>

#include "../../app/app.h"
#include "../control.h"
#include "../../base/cstr.h"
#include "../cursormgr.h"
#include "../../app/event.h"
#include "../../app/evthandler.h"
#include "../../base/list.h"
#include "../../base/memory.h"
#include "../menubar.h"
#include "../../draw/palette.h"
#include "../../app/replay.h"
#include "../scaler.h"
#include "../../draw/screendc.h"
#include "../view.h"
#include "../../draw/windc.h"
#include "../windowborder.h"

static struct obarray *g_window_list;
static int32_t g_window_count;
static int32_t g_next_window_id;

bool g_mouse_captured;

const struct window_ops WINDOW_OPS_TABLE = {
	{window_destroy},
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

static struct control *rewind_ctrl_list(struct window *self)
{
	return OblistFirst(self->ctrl_list);
}

static bool focus_eligible(struct control *c)
{
	return c->base.enabled && c->base.base.handled && (c->flags & 2);
}

static bool is_default_ctrl(struct control *c)
{
	return c->base.enabled && c->base.base.handled && c->is_default;
}

static void register_window(struct window *self)
{
	if (!g_window_list)
		g_window_list = ObarrayCreate(NULL);

	ObarrayAppend(g_window_list, self);
}

void WindowInit(struct window *self, struct view *view)
{
	ObjectInit(&self->base);
	self->base.ops = &WINDOW_OPS_TABLE.base;

	self->root_view = view;
	self->win_type = 0;
	self->shown = false;

	++g_window_count;
	self->window_id = g_next_window_id++;
	self->parent = NULL;
	self->focus_ctrl = NULL;
	self->trigger_event = NULL;
	self->ctrl_list = OblistCreate(NULL);
	self->hwnd = NULL;

	if (view) {
		struct rect f = VIEW_OPS(view)->GetBounds(view);
		self->rect = f;

		int32_t width = self->rect.right - self->rect.left;
		int32_t height = self->rect.bottom - self->rect.top;
		self->rect.top = 0;
		self->rect.left = 0;
		self->rect.right = width;
		self->rect.bottom = height;

	} else {
		self->rect.right = self->rect.left;
		self->rect.bottom = self->rect.top;
	}

	self->windc = WinDcCreate();

	if (self->root_view)
		self->root_view->base.eh_event.event_code = self->window_id;

	register_window(self);
}

void WindowInitSize(struct window *self, struct view *view, int32_t w, int32_t h)
{
	ObjectInit(&self->base);
	self->base.ops = &WINDOW_OPS_TABLE.base;

	self->root_view = view;
	self->win_type = 0;
	self->shown = false;

	++g_window_count;
	self->window_id = g_next_window_id++;
	self->parent = NULL;
	self->focus_ctrl = NULL;
	self->trigger_event = NULL;
	self->ctrl_list = OblistCreate(NULL);
	self->hwnd = NULL;

	self->rect.right = self->rect.left + w;
	self->rect.bottom = self->rect.top + h;

	self->windc = WinDcCreate();

	if (self->root_view)
		self->root_view->base.eh_event.event_code = self->window_id;

	register_window(self);
}

void window_destroy(struct object *obj)
{
	struct window *self = (struct window *) obj;

	--g_window_count;
	WindowHide(self);

	if (self->root_view)
		ObjectDelete(self->root_view);

	if (self->ctrl_list)
		OblistDestroy(&self->ctrl_list);

	if (self->windc)
		WinDcDestroy(&self->windc);

	if (self->hwnd) {
		ScalerDetach(self->hwnd);
		RemovePropA((HWND) self->hwnd, "CWL");
		RemovePropA((HWND) self->hwnd, "CWH");
		DestroyWindow((HWND) self->hwnd);
		self->hwnd = NULL;
	}

	if (g_window_list) {
		int32_t index = ObarrayIndexOf(g_window_list, self);

		if (index > -1)
			ObarrayRemoveAt(g_window_list, index);

		if (ObarrayCount(g_window_list) == 0)
			ObarrayDestroy(&g_window_list);
	}

	object_destroy(obj);
}

struct view *WindowSetRootView(struct window *self, struct view *view)
{
	struct view *old = self->root_view;
	self->root_view = view;

	if (old)
		VIEW_OPS(old)->SetDevice(old, NULL);

	if (self->root_view) {
		struct rect bounds = VIEW_OPS(self->root_view)->GetBounds(self->root_view);
		self->rect = bounds;

		int32_t w = self->rect.right - self->rect.left;
		int32_t h = self->rect.bottom - self->rect.top;
		self->rect.top = 0;
		self->rect.left = 0;
		self->rect.right = w;
		self->rect.bottom = h;

		VIEW_OPS(self->root_view)->SetDevice(self->root_view, self);
	}

	return old;
}

void WindowBringToFront(struct window *self)
{
	if (g_window_list) {
		int32_t count = ObarrayCount(g_window_list);

		for (int32_t index = 0; index < count; ++index) {
			struct window *win = ObarrayGetAt(g_window_list, index);

			if (self != win) {
				if (win->shown)
					WindowIdleRelease(win);
			}
		}
	}

	if (self->hwnd)
		WindowShowV(self);
}

struct window *GetActiveWindowObj(void)
{
	HWND hwnd = GetActiveWindow();

	if (!hwnd)
		return NULL;

	return WindowFromHwnd(hwnd);
}

void WindowAddFocusCtrl(struct window *self, struct control *ctrl, int32_t focus_order)
{
	if (focus_order > 0) {
		struct control *c = rewind_ctrl_list(self);

		if (c) {
			while ((c = OblistNext(self->ctrl_list, false)) != NULL) {
				if (ControlGetFocusIndex(c) >= focus_order)
					break;

				c = OblistNext(self->ctrl_list, true);
			}
		}

		OblistInsertAtCursor(self->ctrl_list, ctrl);

	} else if (focus_order == 0) {
		OblistPushFront(self->ctrl_list, ctrl);

	} else {
		OblistPushBack(self->ctrl_list, ctrl);
	}

	if (!self->focus_ctrl)
		WindowSetFocusCtrl(self, ctrl);
}

void WindowInsertCtrl(struct window *self, struct control *ctrl, int32_t focus_order)
{
	OblistRemove(self->ctrl_list, ctrl);
	WindowAddFocusCtrl(self, ctrl, focus_order);
}

struct control *WindowFindCtrlById(struct window *self, int32_t id)
{
	struct control *c = rewind_ctrl_list(self);

	while (c && c->base.base.eh_event.id != id)
		c = OblistNext(self->ctrl_list, true);

	return c;
}

struct view *WindowFindByTypeId(struct window *self, int32_t code, int32_t id)
{
	struct view *result = NULL;
	struct event spec = EventMake(code, id, -1);

	if (self->root_view)
		result = ViewFindByName(self->root_view, spec);

	return result;
}

void WindowFocusNext(struct window *self)
{
	struct control *c;

	if (self->focus_ctrl) {
		OblistSeekTo(self->ctrl_list, self->focus_ctrl);
		c = OblistNext(self->ctrl_list, true);

		while (c) {
			if (WindowSetFocusCtrl(self, c))
				return;

			c = OblistNext(self->ctrl_list, true);
		}
	}

	c = rewind_ctrl_list(self);

	while (c) {
		if (WindowSetFocusCtrl(self, c))
			return;

		c = OblistNext(self->ctrl_list, true);
	}

	WindowSetFocusCtrl(self, NULL);
}

void WindowFocusPrev(struct window *self)
{
	struct control *c;

	if (self->focus_ctrl) {
		OblistSeekTo(self->ctrl_list, self->focus_ctrl);
		c = OblistPrev(self->ctrl_list, true);

		while (c) {
			if (WindowSetFocusCtrl(self, c))
				return;

			c = OblistPrev(self->ctrl_list, true);
		}
	}

	int32_t count = OblistCount(self->ctrl_list);
	c = count > 0 ? OblistDataAt(self->ctrl_list, count - 1, true) : NULL;

	while (c) {
		if (WindowSetFocusCtrl(self, c))
			return;

		c = OblistPrev(self->ctrl_list, true);
	}

	WindowSetFocusCtrl(self, NULL);
}

bool WindowRemoveFocusCtrl(struct window *self, struct control *ctrl)
{
	if (self->focus_ctrl == ctrl) {
		WindowFocusNext(self);

		if (self->focus_ctrl == ctrl)
			self->focus_ctrl = NULL;
	}

	return OblistRemove(self->ctrl_list, ctrl);
}

bool WindowSetFocusCtrl(struct window *self, struct control *ctrl)
{
	if (ctrl && !EVT_HANDLER_OPS(ctrl)->IsHandled((struct evt_handler *) ctrl))
		return false;

	struct control *prev_focus = self->focus_ctrl;

	if (ctrl && ctrl != prev_focus) {
		struct event spec = EventMake(EVCLS_ANY, -1, -1);
		struct cmd_event *ev;

		if (prev_focus) {
			spec = prev_focus->base.base.eh_event;

			if (focus_eligible(ctrl)) {
				ev = CmdEventCreateFlag(CMD_FOCUS_LOST, false);
				ev->spec_a = spec;
				DispatchGlobalEvent(ev);

			} else {
				ev = CmdEventCreateFlag(CMD_FOCUS_KEPT, false);
				ev->spec_a = spec;
				DispatchGlobalEvent(ev);

				return false;
			}
		}

		if (ctrl && focus_eligible(ctrl)) {
			self->focus_ctrl = ctrl;

			if (self->focus_ctrl->hwnd)
				SetFocus((HWND) self->focus_ctrl->hwnd);

			spec = self->focus_ctrl->base.base.eh_event;
			ev = CmdEventCreateFlag(CMD_FOCUS_GAINED, false);
			ev->spec_a = spec;
			DispatchGlobalEvent(ev);
		}
	}

	return true;
}

void WindowFocusIfEligible(struct window *self, struct control *ctrl)
{
	if (ctrl->base.owner_window == self && focus_eligible(ctrl) &&
		EVT_HANDLER_OPS(ctrl)->IsHandled((struct evt_handler *) ctrl))
	{
		WindowSetFocusCtrl(self, ctrl);
	}
}

int32_t WindowCenterMethod(struct window *self)
{
	return WindowCenter(self);
}

bool window_post_close_event(struct window *self)
{
	if (self->trigger_event) {
		struct cmd_event *ev = CmdEventCreate(CMD_CLOSE_WINDOW);
		ev->param1 = (intptr_t) self;
		ev->spec_a = *self->trigger_event;
		DispatchGlobalEvent(ev);

		return false;
	}

	return true;
}

void WindowHide(struct window *self)
{
	if (self->hwnd) {
		if (self->shown) {
			if (self->parent) {
				if (self->win_type == 2 || self->win_type == 3) {
					EnableWindow((HWND) self->parent->hwnd, TRUE);
					SetFocus((HWND) self->parent->hwnd);
				}
			}

			ShowWindow((HWND) self->hwnd, SW_HIDE);
			self->shown = false;
		}
	}
}

void WindowShowV(struct window *self)
{
	if (self->hwnd) {
		if (!self->shown) {
			ShowWindow((HWND) self->hwnd, SW_SHOWNORMAL);
			self->shown = true;

			if (self->parent) {
				if (self->win_type == 2 || self->win_type == 3)
					EnableWindow((HWND) self->parent->hwnd, FALSE);
			}
		}
	}
}

void window_draw(struct window *self, const struct rect *rect)
{
	if (!self->root_view || !self->windc)
		return;

	if (!rect) {
		struct rect client_rect = ScreenDcScreenToClient(&self->windc->base, &self->rect);
		VIEW_OPS(self->root_view)->DrawChildren(self->root_view, &self->windc->base, &client_rect);

	} else {
		VIEW_OPS(self->root_view)->DrawChildren(self->root_view, &self->windc->base, rect);
	}
}

bool window_is_dialog(struct window *self)
{
	(void) self;

	return false;
}

bool window_vf14(struct window *self, int32_t param)
{
	(void) self;
	(void) param;

	return false;
}

bool window_vf0c(struct window *self)
{
	(void) self;

	return false;
}

bool window_vf08(struct window *self)
{
	(void) self;

	return false;
}

bool window_vf04(struct window *self)
{
	(void) self;

	return false;
}

void window_on_deactivate(struct window *self)
{
	(void) self;
}

void window_on_activate(struct window *self)
{
	(void) self;
}

struct cstr *window_get_text(struct window *self)
{
	int32_t len = GetWindowTextLengthA((HWND) self->hwnd);

	if (len == 0)
		return NULL;

	char *buf = MemAlloc((size_t) len + 1, 1);
	GetWindowTextA((HWND) self->hwnd, buf, len + 1);
	struct cstr *text = CStrCreateFrom(buf);
	MemFree(buf);

	return text;
}

void WindowCreate(struct window *self, int32_t win_type, struct cstr *title, uint32_t style, uint32_t ex_style,
	struct window *parent)
{
	HWND parent_hwnd = NULL;
	HMENU menu = NULL;
	const char *window_name = NULL;

	if (self->hwnd)
		return;

	self->win_type = win_type;
	self->parent = parent;

	if (parent)
		parent_hwnd = (HWND) parent->hwnd;

	if (title)
		window_name = CStrGet(title);

	if (win_type == 1)
		menu = (HMENU) MenuBarGetHandle();

	RECT adj;
	adj.left = self->rect.left;
	adj.top = self->rect.top;
	adj.right = self->rect.right;
	adj.bottom = self->rect.bottom;

	if (win_type != 4)
		AdjustWindowRectEx(&adj, style, menu != NULL, ex_style);

	HWND hwnd;

	if (win_type == 2 || win_type == 3) {
		hwnd = CreateWindowExA(ex_style, "MECCDClass", window_name, style, adj.left, adj.top, adj.right - adj.left,
			adj.bottom - adj.top, parent_hwnd, menu, (HINSTANCE) g_hinstance, NULL);

	} else {
		hwnd = CreateWindowExA(ex_style, "MECCWClass", window_name, style, adj.left, adj.top, adj.right - adj.left,
			adj.bottom - adj.top, parent_hwnd, menu, (HINSTANCE) g_hinstance, NULL);
	}

	self->hwnd = hwnd;

	if (style == 4) {
		struct rect outer = RectMake(adj.top, adj.left, adj.bottom, adj.right);
		GetWindowOuterRect(hwnd, &outer);
		adj.left = outer.left;
		adj.top = outer.top;
		adj.right = outer.right;
		adj.bottom = outer.bottom;
		SetWindowPos(hwnd, NULL, 0, 0, adj.right - adj.left, adj.bottom - adj.top, 0x16);
	}

	ScalerWindowCreated(hwnd, self->rect.right - self->rect.left, self->rect.bottom - self->rect.top);
	WindowCenter(self);

	HDC hdc = GetDC(hwnd);
	WinDcInitDc(self->windc, ScalerAttach(hwnd, hdc), &self->rect, g_palette, false);
	ReleaseDC(hwnd, hdc);

	SetPropA(hwnd, "CWL", (HANDLE) self);
	VIEW_OPS(self->root_view)->SetDevice(self->root_view, self);
}

void NotifyWindowsPalette(struct palette *palette_obj)
{
	if (!g_window_list)
		return;

	int32_t count = ObarrayCount(g_window_list);

	for (int32_t index = 0; index < count; ++index) {
		struct window *win = ObarrayGetAt(g_window_list, index);
		WindowSelectPalette(win, palette_obj);
	}
}

void WindowSelectPalette(struct window *self, struct palette *palette_obj)
{
	ScreenDcSelectPalette(&self->windc->base, palette_obj);
}

bool IsRegisteredWindow(const void *obj)
{
	if (g_window_list) {
		if (ObarrayIndexOf(g_window_list, obj) > -1)
			return true;

		return false;
	}

	return false;
}

void NotifyWindowsDeactivate(void)
{
	if (!g_window_list)
		return;

	int32_t count = ObarrayCount(g_window_list);

	for (int32_t index = 0; index < count; ++index) {
		struct window *win = ObarrayGetAt(g_window_list, index);
		WINDOW_OPS(win)->OnDeactivate(win);
	}
}

void NotifyWindowsActivate(void)
{
	if (!g_window_list)
		return;

	int32_t count = ObarrayCount(g_window_list);

	for (int32_t index = 0; index < count; ++index) {
		struct window *win = ObarrayGetAt(g_window_list, index);
		WINDOW_OPS(win)->OnActivate(win);
	}
}

struct window *WindowFromHwnd(void *hwnd)
{
	struct window *win = (struct window *) GetPropA((HWND) hwnd, "CWL");

	if (IsRegisteredWindow(win))
		return win;

	return NULL;
}

struct point WindowCursorToClient(struct window *self)
{
	struct point pt = {0};

	RpGetCursorPos(&pt);
	ScalerScreenToWindow(self->hwnd, &pt);

	return pt;
}

void WindowValidate(struct window *self, const struct rect *rect)
{
	if (self->shown) {
		if (!rect) {
			ValidateRect((HWND) self->hwnd, NULL);

		} else {
			struct rect r = *rect;
			ScalerRectToClient(&r);

			RECT wr;
			wr.left = r.left;
			wr.top = r.top;
			wr.right = r.right;
			wr.bottom = r.bottom;
			ValidateRect((HWND) self->hwnd, &wr);
		}
	}
}

void WindowInvalidate(struct window *self, const struct rect *rect)
{
	if (self->shown) {
		if (!rect) {
			InvalidateRect((HWND) self->hwnd, NULL, FALSE);

		} else {
			struct rect r = *rect;
			ScalerRectToClient(&r);

			RECT wr;
			wr.left = r.left;
			wr.top = r.top;
			wr.right = r.right;
			wr.bottom = r.bottom;
			InvalidateRect((HWND) self->hwnd, &wr, FALSE);
		}
	}
}

void WindowIdleRelease(struct window *self)
{
	if (g_mouse_captured) {
		struct view *focus_obj = g_focus_object;

		struct point pt = {0};

		if (focus_obj) {
			struct rect bounds = VIEW_OPS(focus_obj)->GetBounds(focus_obj);
			pt = RectTopLeft(&bounds);
		}

		pt.v -= 100;
		pt.h -= 100;
		WINDOW_OPS(self)->HandleMouse(self, false, &pt, MOUSE_UP, 0, 0);
	}

	CursorSetId(CURSOR_ARROW);
}

void window_handle_key(struct window *self, int32_t key, int32_t mods)
{
	key = (uint8_t) key;

	struct event spec = EventMake(EVCLS_ANY, -1, -1);
	struct cmd_event *ev;

	if (key == 13 || key == 3) {
		bool swallow_enter = false;

		if (key == 13 && self->focus_ctrl)
			swallow_enter = self->focus_ctrl->wants_enter;

		if (!swallow_enter) {
			struct control *c = rewind_ctrl_list(self);

			while (c) {
				if (is_default_ctrl(c)) {
					spec = c->base.base.eh_event;
					ev = CmdEventCreateFlag(CMD_KEY_DOWN, false);
					ev->spec_a = spec;
					ev->param1 = key;
					ev->param2 = mods;
					DispatchGlobalEvent(ev);

					return;
				}

				c = OblistNext(self->ctrl_list, true);
			}
		}
	}

	if (key == 9) {
		if (mods & 0x40) {
			WindowFocusPrev(self);

		} else {
			WindowFocusNext(self);
		}

		return;
	}

	if (self->focus_ctrl) {
		spec = self->focus_ctrl->base.base.eh_event;
		ev = CmdEventCreateFlag(CMD_KEY_DOWN, false);
		ev->spec_a = spec;
		ev->param1 = key;
		ev->param2 = mods;
		DispatchGlobalEvent(ev);
	}
}

static struct point g_lastpt;

void window_handle_mouse(struct window *self, bool down, const struct point *pt, int32_t kind, int32_t param2,
	int32_t param1)
{
	struct event spec = EventMake(EVCLS_ANY, -1, -1);
	int32_t mouse_action = 3;

	if (!self->root_view)
		return;

	bool was_captured = g_mouse_captured;
	struct view *hit_view;

	if (ViewPointInBounds(self->root_view, pt)) {
		hit_view = ViewFindChildAt(self->root_view, *pt);

	} else {
		hit_view = NULL;
	}

	g_lastpt = *pt;

	switch (kind) {
		case MOUSE_DOWN:
			if (self->hover_child != hit_view && param1) {
				self->hover_child = hit_view;
				param1 = 0;
			}

			if (!was_captured) {
				if (hit_view && (struct view *) self->focus_ctrl != hit_view &&
					OblistSeekTo(self->ctrl_list, hit_view))
				{
					WindowSetFocusCtrl(self, (struct control *) hit_view);
				}

				mouse_action = 1;
				SetFocusObject(hit_view, true);
				g_mouse_captured = true;
				SetCapture((HWND) self->hwnd);
			}
			break;
		case MOUSE_UP:
			if (was_captured) {
				mouse_action = 2;
				g_mouse_captured = false;
				ReleaseCapture();
				hit_view = SetFocusObject(hit_view, false);

			} else {
				return;
			}
			break;
		default:
			mouse_action = 3;

			if (!down) {
				hit_view = SetFocusObject(hit_view, false);

			} else {
				hit_view = g_focus_object;
			}

			if (!was_captured)
				return;
			break;
	}

	if (hit_view) {
		spec = hit_view->base.eh_event;
		struct mouse_event *ev = MouseEventCreate(mouse_action, down, pt, 0);
		ev->base.spec_a = spec;
		ev->base.param1 = param1;
		ev->base.param2 = param2;
		DispatchGlobalEvent(&ev->base);
	}
}

void window_post_mouse_event(struct window *self, int32_t command, int32_t param1, int32_t param2)
{
	if (self->focus_ctrl) {
		struct event spec = self->focus_ctrl->base.base.eh_event;
		struct cmd_event *ev = CmdEventCreateFlag(command, false);
		ev->spec_a = spec;
		ev->param1 = param1;
		ev->param2 = param2;
		DispatchGlobalEvent(ev);
	}
}

struct view *SetFocusObject(struct view *obj, bool play_sound)
{
	struct view *old = g_focus_object;

	if (old != obj || play_sound) {
		if (obj) {
			CursorSetId(obj->cursor_id);

		} else {
			CursorSetId(CURSOR_ARROW);
		}

		g_focus_object = obj;
		obj = old;
	}

	return obj;
}

int32_t WindowCenter(struct window *self)
{
	int32_t cx;
	int32_t cy;
	RECT r;

	if (self->parent) {
		GetWindowRect((HWND) self->parent->hwnd, &r);
		cx = r.left + (r.right - r.left) / 2;
		cy = r.top + (r.bottom - r.top) / 2;

	} else {
		cx = GetSystemMetrics(SM_CXSCREEN) / 2;
		cy = GetSystemMetrics(SM_CYSCREEN) / 2;
	}

	GetWindowRect((HWND) self->hwnd, &r);

	return SetWindowPos((HWND) self->hwnd, NULL, cx - (r.right - r.left) / 2, cy - (r.bottom - r.top) / 2, 0, 0,
		0x15);
}

void WindowSetTextCStr(struct window *self, struct cstr *text)
{
	char buf[256];

	CStrCopyToBuffer(text, buf, sizeof(buf));
	SetWindowTextA((HWND) self->hwnd, buf);
	CStrDestroy(&text);
}

void WindowSetTitleCStr(struct window *self, struct cstr *text)
{
	char buf[256];

	buf[0] = 0;
	CStrCopyToBuffer(text, buf, sizeof(buf));
	SetWindowTextA((HWND) self->hwnd, buf);
	CStrDestroy(&text);
}
