#include "../button.h"

#include <stddef.h>

#include <windows.h>

#include "../../app/app.h"
#include "../../base/cstr.h"
#include "../../draw/dc.h"
#include "../../draw/dispobj.h"
#include "../../draw/font.h"
#include "../../draw/fonttable.h"
#include "../../data/gfxdev.h"
#include "../../base/list.h"
#include "../../base/memory.h"
#include "../../draw/region.h"
#include "../scaler.h"
#include "../../draw/screendc.h"
#include "../../data/strrsrc.h"
#include "../../draw/textlabel.h"
#include "../../draw/windc.h"
#include "../window.h"

static const int32_t g_button_handler_table[6] = {
	CMD_MOUSE, CMD_BUTTON_HILITE, CMD_BUTTON_TOGGLE, CMD_FOCUS_GAINED, CMD_FOCUS_LOST, CMD_KEY_DOWN,
};

static LRESULT CALLBACK ctrl_subclass_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

const struct button_ops BUTTON_OPS_TABLE = {
	{
		{
			{
				{button_destroy},
				evt_handler_is_handled,
				control_set_handled_for,
				control_set_handled,
				control_clear_handled_for,
				control_clear_handled,
				button_on_event,
				view_on_idle,
				view_on_idle_spare,
			},
			view_add_child,
			button_get_bounds,
			view_get_clip_region,
			button_hide,
			view_remove_child,
			view_remove_from_parent,
			view_set_cursor_id,
			control_set_device,
			button_show,
			button_draw_children,
			view_move_children,
		},
		button_set_value,
		control_on_command,
		control_focus_sibling,
		button_on_key_down,
		control_set_focus,
		button_create_or_destroy,
	},
	button_on_char,
	button_on_button_down,
	button_on_button_up,
	button_on_mouse_move,
};

static struct text_label *data_label(struct button *self)
{
	return (struct text_label *) self->data_src;
}

void ButtonInit(struct button *self, int32_t id, const struct event *event, const struct rect *rect,
	struct cstr *label, int32_t ctrl_type, int32_t value, int32_t font)
{
	ControlInit(&self->base, ctrl_type, id, event, rect, 2, -1);
	self->base.base.base.base.ops = &BUTTON_OPS_TABLE.base.base.base.base;

	ButtonInitButton(self, label, value, ctrl_type, font);
	EvtHandlerBindHandler(&self->base.base.base, 6, g_button_handler_table);
}

void ButtonInitPoint(struct button *self, int32_t id, const struct event *event_src, const struct point *pos,
	struct cstr *label, int32_t ctrl_type, int32_t value, int32_t font)
{
	ControlInitPoint(&self->base, ctrl_type, id, event_src, pos, 2, -1);
	self->base.base.base.base.ops = &BUTTON_OPS_TABLE.base.base.base.base;

	ButtonInitButton(self, label, value, ctrl_type, font);
	EvtHandlerBindHandler(&self->base.base.base, 6, g_button_handler_table);
}

void ButtonInitDispObj(struct button *self, int32_t id, const struct event *event, struct disp_obj *data_obj,
	const struct rect *rect, int32_t ctrl_type, int32_t init_value)
{
	ControlInit(&self->base, ctrl_type, id, event, rect, 2, -1);
	self->base.base.base.base.ops = &BUTTON_OPS_TABLE.base.base.base.base;

	ButtonInitData(self, data_obj, init_value, ctrl_type);
	EvtHandlerBindHandler(&self->base.base.base, 6, g_button_handler_table);
}

static void adjust_both_rects(const struct rect *size_rect, struct rect *ra, struct rect *rb)
{
	int32_t height = size_rect->bottom - size_rect->top;
	int32_t width = size_rect->right - size_rect->left;

	int32_t a_right = width + ra->left;
	int32_t a_bottom = height + ra->top;
	ra->right = a_right;
	ra->bottom = a_bottom;

	int32_t b_right = width + rb->left;
	int32_t b_bottom = height + rb->top;
	rb->right = b_right;
	rb->bottom = b_bottom;
}

void ButtonInitDispObjPoint(struct button *self, int32_t id, const struct event *event_src, struct disp_obj *rect_src,
	const struct point *pos, int32_t control_type, int32_t value)
{
	ControlInitPoint(&self->base, control_type, id, event_src, pos, 2, -1);
	self->base.base.base.base.ops = &BUTTON_OPS_TABLE.base.base.base.base;

	if (rect_src) {
		struct text_label *label = (struct text_label *) rect_src;
		struct rect size_rect = TEXT_LABEL_OPS(label)->GetBounds(label);

		adjust_both_rects(&size_rect, &self->base.base.rect_a, &self->base.base.rect_b);

		ObjectDelete(self->base.base.region_bounds);
		self->base.base.region_bounds = RegionCreateRect(&self->base.base.rect_b, 1, 0, 0);
	}

	ButtonInitData(self, rect_src, value, control_type);
	EvtHandlerBindHandler(&self->base.base.base, 6, g_button_handler_table);
}

struct button *ButtonCreateDispObj(int32_t id, const struct event *event, struct disp_obj *data_obj,
	const struct rect *rect, int32_t ctrl_type, int32_t init_value)
{
	struct button *self = MemAlloc(1, sizeof(struct button));
	ButtonInitDispObj(self, id, event, data_obj, rect, ctrl_type, init_value);

	return self;
}

struct button *ButtonCreate(int32_t id, const struct event *event, const struct rect *rect, struct cstr *label,
	int32_t ctrl_type, int32_t value, int32_t font)
{
	struct button *self = MemAlloc(1, sizeof(struct button));
	ButtonInit(self, id, event, rect, label, ctrl_type, value, font);

	return self;
}

struct button *ButtonCreatePoint(int32_t id, const struct event *event_src, const struct point *pos,
	struct cstr *label, int32_t ctrl_type, int32_t value, int32_t font)
{
	struct button *self = MemAlloc(1, sizeof(struct button));
	ButtonInitPoint(self, id, event_src, pos, label, ctrl_type, value, font);

	return self;
}

struct button *ButtonCreateDispObjPoint(int32_t id, const struct event *event_src, struct disp_obj *rect_src,
	const struct point *pos, int32_t control_type, int32_t value)
{
	struct button *self = MemAlloc(1, sizeof(struct button));
	ButtonInitDispObjPoint(self, id, event_src, rect_src, pos, control_type, value);

	return self;
}

void ButtonDestroy(struct button **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void button_destroy(struct object *obj)
{
	struct button *self = (struct button *) obj;

	ControlRelinquishFocus(&self->base);
	EvtHandlerReleaseBinding(&self->base.base.base, 6, g_button_handler_table);

	if (self->base.owns_window) {
		if (self->base.base.owner_window)
			VIEW_OPS(self)->SetDevice(&self->base.base, NULL);
	}

	if (self->label_rsrc)
		CStrDestroy(&self->label_rsrc);

	if (self->data_src)
		ObjectDestroy(&self->data_src);

	obj->ops = &CONTROL_OPS_TABLE.base.base.base;
	control_destroy(obj);
}

static struct cstr *desc_label(struct ctrl_desc *desc)
{
	struct cstr *label = NULL;
	int32_t index = desc->item_index;

	if (index > -1) {
		if (desc->src) {
			label = StrRsrcGetCStr(desc->src, index);

		} else if (desc->rsrc_id > 0) {
			struct strrsrc *strs = StrRsrcLoad(g_text_lib, desc->rsrc_id, true);

			label = strs ? StrRsrcGetCStr(strs, index) : NULL;
			StrRsrcDestroy(&strs);
		}
	}

	return label;
}

static struct rect desc_rect(struct ctrl_desc *desc)
{
	return RectMake(desc->rect.top, desc->rect.left, desc->rect.bottom, desc->rect.right);
}

struct view *CreateControlFromTemplate(struct ctrl_desc *desc)
{
	struct rect rect = desc_rect(desc);
	struct cstr *label = desc_label(desc);

	struct button *result = ButtonCreate(desc->id, desc->event, &rect, label, desc->style, desc->value, 0);
	ControlApplyDescriptor(&result->base, desc);

	return &result->base.base;
}

struct rect button_get_bounds(struct view *self)
{
	return view_get_bounds(self);
}

void button_draw_children(struct view *view, struct screen_dc *dc, const struct rect *bounds)
{
	struct button *self = (struct button *) view;

	if (view->enabled && (self->data_src || self->base.hwnd)) {
		struct rect own = VIEW_OPS(view)->GetBounds(view);
		struct rect rc;

		if (RectIntersect(bounds, &own, &rc)) {
			struct dc clip_dc;
			DcInit(&clip_dc, dc);

			int32_t mode = 0;

			if (!EVT_HANDLER_OPS(view)->IsHandled(&view->base)) {
				mode = 1;

			} else {
				if (self->base.slider_value)
					mode = 2;
			}

			ViewComputeClip(view, dc);

			if (self->data_src) {
				struct rect b = VIEW_OPS(view)->GetBounds(view);
				struct point top_left = RectTopLeft(&b);
				struct text_label *label = data_label(self);
				TEXT_LABEL_OPS(label)->DrawInner(label, dc, &top_left, self->base.value, mode, self->base.is_default);

			} else if (self->base.hwnd) {
				struct rect b = VIEW_OPS(view)->GetBounds(view);
				RECT r;
				r.left = b.left;
				r.top = b.top;
				r.right = b.right;
				r.bottom = b.bottom;
				InvalidateRect((HWND) self->base.hwnd, &r, FALSE);
				UpdateWindow((HWND) self->base.hwnd);
			}

			dc_destroy(&clip_dc.base);
		}
	}

	control_draw_children(view, dc, bounds);
}

void button_hide(struct view *self, bool fill, bool invalidate)
{
	control_hide(self, fill, invalidate);
}

void ButtonInitData(struct button *self, struct disp_obj *arg_data_src, int32_t value, int32_t style)
{
	self->is_toggle = (style == 1 || style == 2);
	CONTROL_OPS(self)->SetValue(&self->base, (int16_t) value, false);
	self->data_src = arg_data_src;

	if (self->data_src) {
		ObjectDelete(self->base.base.region_clip);

		struct rect b = VIEW_OPS(self)->GetBounds(&self->base.base);
		struct point top_left = RectTopLeft(&b);
		struct text_label *label = (struct text_label *) arg_data_src;
		self->base.base.region_clip = TEXT_LABEL_OPS(label)->MakeRegion(label, &top_left);
	}

	self->label_rsrc = NULL;
	self->base.hwnd = NULL;
}

void ButtonInitButton(struct button *self, struct cstr *label, int32_t value, int32_t style, int32_t font)
{
	self->base.owns_window = true;
	self->is_toggle = (style == 1 || style == 2);
	CONTROL_OPS(self)->SetValue(&self->base, (int16_t) value, false);
	self->data_src = NULL;
	self->base.hwnd = NULL;
	self->font_id = font;
	self->label_rsrc = label;

	switch (style) {
		case 0:
		case 3:
		case 4:
			self->base.win_style = 0;
			break;

		case 1:
			self->base.win_style = 4;
			break;

		case 2:
			self->base.win_style = 3;
			break;

		default:
			self->base.win_style = -1;
			break;
	}

	ObjectDelete(self->base.base.region_clip);
	self->base.base.region_clip = RegionCreate();
}

static void trigger_event(struct button *self, struct cmd_event *cmd)
{
	EVT_HANDLER_OPS(self)->OnEvent(&self->base.base.base, cmd);
	CmdEventDestroy(&cmd);
}

static struct cmd_event *toggle_event(struct button *self, int32_t command)
{
	return CmdEventCreateSpecsFlag(command, &self->base.base.base.eh_event, &self->base.base.base.eh_event, false);
}

static struct obarray *sibling_list(struct button *self)
{
	if (self->base.base.parent && self->base.base.parent->has_child_list)
		return self->base.base.parent->child_list;

	return NULL;
}

static void redraw(struct button *self, struct screen_dc *dc)
{
	struct rect bounds = VIEW_OPS(self)->GetBounds(&self->base.base);
	VIEW_OPS(self)->DrawChildren(&self->base.base, dc, &bounds);
}

void button_on_event(struct evt_handler *handler, struct cmd_event *event)
{
	struct button *self = (struct button *) handler;
	struct screen_dc *dc = NULL;

	if (self->base.base.owner_window)
		dc = &self->base.base.owner_window->windc->base;

	switch (event->command) {
		case CMD_KEY_DOWN: {
			event->handled = true;
			int32_t key = (int8_t) event->param1;

			if (key == VK_RETURN || key == VK_CANCEL) {
				if (self->is_toggle) {
					if (self->base.control_type != CTRL_RADIO || !self->base.value) {
						struct cmd_event *cmd = toggle_event(self, CMD_BUTTON_TOGGLE);
						cmd->param1 = 1;
						trigger_event(self, cmd);
					}
				}

				if (dc) {
					self->base.slider_value = 1;
					redraw(self, dc);
					self->base.slider_value = 0;
					redraw(self, dc);
				}

				ControlFireCommand(&self->base);
			}
			break;
		}

		case CMD_MOUSE: {
			struct mouse_event *mouse = (struct mouse_event *) event;
			event->handled = true;

			switch (mouse->kind) {
				case MOUSE_DOWN: {
					struct point pt = mouse->mouse_pos;

					if (ViewPointInBounds(&self->base.base, &pt)) {
						self->base.slider_value = mouse->button_down;

						if (dc)
							redraw(self, dc);

						if (self->base.control_type == CTRL_HOTSPOT || self->base.control_type == CTRL_TEXT_BUTTON)
							ControlFireCommand(&self->base);
					}
					break;
				}

				case MOUSE_UP: {
					struct point pt = mouse->mouse_pos;

					if (ViewPointInBounds(&self->base.base, &pt)) {
						self->base.slider_value = mouse->button_down;

						if (self->is_toggle) {
							if (self->base.control_type != CTRL_RADIO || !self->base.value) {
								struct cmd_event *cmd = toggle_event(self, CMD_BUTTON_TOGGLE);
								cmd->param1 = 1;
								trigger_event(self, cmd);
							}
						}

						if (dc)
							redraw(self, dc);

						if (self->base.control_type != CTRL_HOTSPOT)
							ControlFireCommand(&self->base);
					}
					break;
				}

				case MOUSE_MOVE: {
					struct point pt = mouse->mouse_pos;

					if ((int32_t) ViewPointInBounds(&self->base.base, &pt) != self->base.slider_value) {
						struct cmd_event *cmd = toggle_event(self, CMD_BUTTON_HILITE);
						cmd->param1 = self->base.slider_value == 0;
						trigger_event(self, cmd);
					}
					break;
				}
			}
			break;
		}

		case CMD_BUTTON_HILITE:
			event->handled = true;

			if (self->base.slider_value != event->param1) {
				self->base.slider_value = (int32_t) event->param1;

				if (dc)
					redraw(self, dc);
			}
			break;

		case CMD_BUTTON_TOGGLE: {
			event->handled = true;

			if (self->base.control_type == CTRL_CHECKBOX) {
				if (!self->base.value) {
					self->base.value = 1;

				} else {
					self->base.value = 0;
				}
			}

			if (self->base.control_type == CTRL_RADIO) {
				if (!self->base.value) {
					bool notify = (uint8_t) event->param1 != 0;
					CONTROL_OPS(self)->SetValue(&self->base, 1, notify);

					int32_t child_index = 0;
					struct button *child;

					do {
						child = sibling_list(self) ? ObarrayGetAt(sibling_list(self), child_index++) : NULL;

						if (child) {
							if (self != child) {
								if (child->base.base.base.eh_event.code == EVCLS_CONTROL) {
									struct control *sibling = &child->base;

									if (sibling->control_type == CTRL_RADIO) {
										if (sibling->value) {
											CONTROL_OPS(sibling)->SetValue(sibling, 0, notify);
											break;
										}
									}
								}
							}
						}
					} while (child);
				}
			}
			break;
		}

		default:
			control_on_event(handler, event);
			break;
	}
}

void button_create_or_destroy(struct control *ctrl, struct window *parent)
{
	struct button *self = (struct button *) ctrl;

	if (!ctrl->owns_window)
		return;

	if (parent) {
		if (ctrl->win_style != -1) {
			const char *window_name = NULL;
			struct rect view_c = VIEW_OPS(self)->GetBounds(&ctrl->base);

			if (self->label_rsrc) {
				if (CStrLength(self->label_rsrc) > 0)
					window_name = CStrGet(self->label_rsrc);
			}

			if (ctrl->is_default && ctrl->win_style == 0)
				ctrl->win_style = 1;

			HWND hwnd = CreateWindowExA(0, "BUTTON", window_name, (DWORD) ctrl->win_style | 0x50000000,
				view_c.left - 1, view_c.top - 1, (view_c.right - view_c.left) + 2, (view_c.bottom - view_c.top) + 2,
				(HWND) parent->hwnd, NULL, (HINSTANCE) g_hinstance, NULL);
			ctrl->hwnd = hwnd;

			SetPropA(hwnd, "CCLO", (HANDLE) self);
			ctrl->orig_wndproc = (void *) SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR) ctrl_subclass_wndproc);

			struct font *font = ObarrayGetAt(g_font_table, self->font_id);
			SendMessageA(hwnd, WM_SETFONT, (WPARAM) font->hfont, 0);
			ScalerChild(hwnd, view_c.left - 1, view_c.top - 1, (view_c.right - view_c.left) + 2,
				(view_c.bottom - view_c.top) + 2, NULL);
			SendMessageA(hwnd, 0xF1, (WPARAM) ctrl->value, 0);
		}

	} else {
		if (ctrl->hwnd) {
			RemovePropA((HWND) ctrl->hwnd, "CCLO");
			RemovePropA((HWND) ctrl->hwnd, "CCHI");
			DestroyWindow((HWND) ctrl->hwnd);
			ctrl->hwnd = NULL;
		}
	}
}

void button_set_value(struct control *ctrl, int32_t checked, bool notify)
{
	if (ctrl->hwnd)
		SendMessageA((HWND) ctrl->hwnd, 0xF1, checked != 0, 0);

	control_set_value(ctrl, checked, notify);
}

void ButtonSetData(struct button *self, struct cstr *data, bool redraw_now)
{
	struct cstr *old = NULL;

	if (self->data_src) {
		struct text_label *label = data_label(self);
		TEXT_LABEL_OPS(label)->SetText(label, data);

	} else if (self->base.owns_window) {
		old = self->label_rsrc;
		self->label_rsrc = data;

	} else if (data) {
		old = data;
	}

	if (old)
		CStrDestroy(&old);

	if (redraw_now) {
		struct screen_dc *dc = ViewGetWinDc(&self->base.base);

		if (dc)
			redraw(self, dc);

	} else {
		ViewNotifyChanged(&self->base.base);
	}
}

void button_show(struct view *self)
{
	control_show(self);
}

intptr_t ButtonDispatchWndMsg(struct button *self, void *hwnd, uint32_t msg, uintptr_t wparam, intptr_t lparam)
{
	void *own = self->base.hwnd;
	int32_t lo = (int16_t) LOWORD(lparam);
	int32_t hi = (int16_t) HIWORD(lparam);

	switch (msg) {
		case 0x102:
			BUTTON_OPS(self)->OnChar(self, own, (int8_t) wparam, lo);
			return 0;

		case 0x201:
			BUTTON_OPS(self)->OnButtonDown(self, own, 0, lo, hi, (uint32_t) wparam);
			return 0;

		case 0x203:
			BUTTON_OPS(self)->OnButtonDown(self, own, 1, lo, hi, (uint32_t) wparam);
			return 0;

		case 0x202:
			BUTTON_OPS(self)->OnButtonUp(self, own, lo, hi, (uint32_t) wparam);
			return 0;

		case 0x200:
			BUTTON_OPS(self)->OnMouseMove(self, own, lo, hi, (uint32_t) wparam);
			return 0;

		case 0x100:
			CONTROL_OPS(self)->OnKeyDown(&self->base, own, (uint32_t) wparam, 1, lo, (uint16_t) ((uint32_t) lparam >> 16));
			return 0;

		default:
			return CallWindowProcA((WNDPROC) self->base.orig_wndproc, (HWND) hwnd, msg, wparam, lparam);
	}
}

static LPARAM make_lparam(int32_t lo, int32_t hi)
{
	return (LPARAM) (int32_t) ((uint32_t) (uint16_t) lo | ((uint32_t) (uint16_t) hi << 16));
}

void button_on_key_down(struct control *self, void *hwnd, uint32_t wparam, int32_t reserved, int32_t lparam_lo,
	uint32_t lparam_hi)
{
	(void) reserved;

	ControlCallDefaultWndProc(self, hwnd, 0x100, wparam, make_lparam(lparam_lo, (int32_t) lparam_hi));
}

intptr_t button_on_char(struct button *self, void *hwnd, int32_t key, int32_t lparam_lo)
{
	struct window *win = self->base.base.owner_window;
	WINDOW_OPS(win)->HandleKey(win, key, 0);

	return ControlCallDefaultWndProc(&self->base, hwnd, 0x102, (uintptr_t) (intptr_t) (int8_t) key,
		(uint16_t) lparam_lo);
}

void button_on_button_down(struct button *self, void *hwnd, int32_t dblclk, int32_t lparam_lo, int32_t lparam_hi,
	uint32_t wparam)
{
	struct window *win = self->base.base.owner_window;
	struct point pt = WindowCursorToClient(win);
	WINDOW_OPS(win)->HandleMouse(win, true, &pt, MOUSE_DOWN, 0, 0);

	ControlCallDefaultWndProc(&self->base, hwnd, dblclk == 0 ? 513 : 515, wparam, make_lparam(lparam_lo, lparam_hi));
}

void button_on_button_up(struct button *self, void *hwnd, int32_t lparam_lo, int32_t lparam_hi, uint32_t wparam)
{
	struct window *win = self->base.base.owner_window;
	struct point pt = WindowCursorToClient(win);
	WINDOW_OPS(win)->HandleMouse(win, false, &pt, MOUSE_UP, 0, 0);

	ControlCallDefaultWndProc(&self->base, hwnd, 0x202, wparam, make_lparam(lparam_lo, lparam_hi));
}

void button_on_mouse_move(struct button *self, void *hwnd, int32_t lparam_lo, int32_t lparam_hi, uint32_t wparam)
{
	struct window *win = self->base.base.owner_window;
	struct point pt = WindowCursorToClient(win);
	WINDOW_OPS(win)->HandleMouse(win, (wparam & 1) != 0, &pt, MOUSE_MOVE, 0, 0);

	ControlCallDefaultWndProc(&self->base, hwnd, 0x200, wparam, make_lparam(lparam_lo, lparam_hi));
}

static LRESULT CALLBACK ctrl_subclass_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	struct button *ctrl = GetPropA(hwnd, "CCLO");

	if (ctrl)
		return ButtonDispatchWndMsg(ctrl, hwnd, msg, wparam, lparam);

	return DefWindowProcA(hwnd, msg, wparam, lparam);
}


const struct check_box_ops CHECK_BOX_OPS_TABLE = {
	{
		{
			{
				{
					{check_box_destroy},
					evt_handler_is_handled,
					control_set_handled_for,
					control_set_handled,
					control_clear_handled_for,
					control_clear_handled,
					button_on_event,
					view_on_idle,
					view_on_idle_spare,
				},
				view_add_child,
				button_get_bounds,
				view_get_clip_region,
				button_hide,
				view_remove_child,
				view_remove_from_parent,
				view_set_cursor_id,
				control_set_device,
				button_show,
				button_draw_children,
				view_move_children,
			},
			button_set_value,
			control_on_command,
			control_focus_sibling,
			button_on_key_down,
			control_set_focus,
			button_create_or_destroy,
		},
		button_on_char,
		button_on_button_down,
		button_on_button_up,
		button_on_mouse_move,
	},
};

void CheckBoxInit(struct check_box *self, int32_t id, const struct event *event, const struct rect *rect,
	struct cstr *label, int32_t value, int32_t font)
{
	ButtonInit(&self->base, id, event, rect, label, 2, value, font);
	self->base.base.base.base.base.ops = &CHECK_BOX_OPS_TABLE.base.base.base.base.base;
}

struct check_box *CheckBoxCreate(int32_t id, const struct event *event, const struct rect *rect, struct cstr *label,
	int32_t value, int32_t font)
{
	struct check_box *self = MemAlloc(1, sizeof(struct check_box));
	CheckBoxInit(self, id, event, rect, label, value, font);

	return self;
}

void CheckBoxDestroy(struct check_box **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void check_box_destroy(struct object *self)
{
	self->ops = &BUTTON_OPS_TABLE.base.base.base.base;
	button_destroy(self);
}


const struct clip_button_ops CLIP_BUTTON_OPS_TABLE = {
	{
		{
			{
				{
					{clip_button_destroy},
					evt_handler_is_handled,
					control_set_handled_for,
					control_set_handled,
					control_clear_handled_for,
					control_clear_handled,
					button_on_event,
					view_on_idle,
					view_on_idle_spare,
				},
				view_add_child,
				button_get_bounds,
				view_get_clip_region,
				button_hide,
				view_remove_child,
				view_remove_from_parent,
				view_set_cursor_id,
				control_set_device,
				button_show,
				button_draw_children,
				view_move_children,
			},
			button_set_value,
			control_on_command,
			control_focus_sibling,
			button_on_key_down,
			control_set_focus,
			button_create_or_destroy,
		},
		button_on_char,
		button_on_button_down,
		button_on_button_up,
		button_on_mouse_move,
	},
};

void ClipButtonInit(struct clip_button *self, int32_t id, const struct event *event, const struct rect *rect,
	int32_t command_id, int32_t command_p1, int32_t command_p2)
{
	ButtonInitDispObj(&self->base, id, event, NULL, rect, 4, 0);
	self->base.base.base.base.base.ops = &CLIP_BUTTON_OPS_TABLE.base.base.base.base.base;

	ControlSetCommand(&self->base.base, command_id, command_p1, command_p2);
	ObjectDelete(self->base.base.base.region_clip);
	self->base.base.base.region_clip = RegionCreate();
}

struct clip_button *ClipButtonCreate(int32_t id, const struct event *event, const struct rect *rect,
	int32_t command_id, int32_t command_p1, int32_t command_p2)
{
	struct clip_button *self = MemAlloc(1, sizeof(struct clip_button));
	ClipButtonInit(self, id, event, rect, command_id, command_p1, command_p2);

	return self;
}

void ClipButtonDestroy(struct clip_button **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void clip_button_destroy(struct object *self)
{
	self->ops = &BUTTON_OPS_TABLE.base.base.base.base;
	button_destroy(self);
}

struct view *CtrlCreateFromDesc(struct ctrl_desc *desc)
{
	struct rect rect = desc_rect(desc);

	struct clip_button *button = ClipButtonCreate(desc->id, desc->event, &rect, CMD_CONTROL_ACTION, 0, 0);
	ControlApplyDescriptor(&button->base.base, desc);

	return &button->base.base.base;
}


bool DefBtnKeyFilter(const char *in_key, char *out_key)
{
	bool is_mapped = true;
	char key = *in_key;

	switch (key) {
		case 114:
			key = 13;
			break;

		default:
			is_mapped = false;
			break;
	}

	*out_key = key;

	return is_mapped;
}

const struct def_button_ops DEF_BUTTON_OPS_TABLE = {
	{
		{
			{
				{
					{def_button_destroy},
					evt_handler_is_handled,
					control_set_handled_for,
					control_set_handled,
					control_clear_handled_for,
					control_clear_handled,
					button_on_event,
					view_on_idle,
					view_on_idle_spare,
				},
				view_add_child,
				button_get_bounds,
				view_get_clip_region,
				button_hide,
				view_remove_child,
				view_remove_from_parent,
				view_set_cursor_id,
				control_set_device,
				button_show,
				button_draw_children,
				view_move_children,
			},
			button_set_value,
			control_on_command,
			control_focus_sibling,
			button_on_key_down,
			control_set_focus,
			button_create_or_destroy,
		},
		button_on_char,
		button_on_button_down,
		button_on_button_up,
		button_on_mouse_move,
	},
};

void DefButtonInit(struct def_button *self, int32_t id, const struct event *event, const struct rect *rect,
	struct cstr *label, int32_t value, int32_t font)
{
	ButtonInit(&self->base, id, event, rect, label, 0, value, font);
	self->base.base.base.base.base.ops = &DEF_BUTTON_OPS_TABLE.base.base.base.base.base;
}

struct def_button *DefButtonCreate(int32_t id, const struct event *event, const struct rect *rect,
	struct cstr *label, int32_t value, int32_t font)
{
	struct def_button *self = MemAlloc(1, sizeof(struct def_button));
	DefButtonInit(self, id, event, rect, label, value, font);

	return self;
}

void DefButtonDestroy(struct def_button **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void def_button_destroy(struct object *self)
{
	self->ops = &BUTTON_OPS_TABLE.base.base.base.base;
	button_destroy(self);
}


const struct dirty_button_ops DIRTY_BUTTON_OPS_TABLE = {
	{
		{
			{
				{
					{dirty_button_destroy},
					evt_handler_is_handled,
					control_set_handled_for,
					control_set_handled,
					control_clear_handled_for,
					control_clear_handled,
					button_on_event,
					view_on_idle,
					view_on_idle_spare,
				},
				view_add_child,
				button_get_bounds,
				view_get_clip_region,
				button_hide,
				view_remove_child,
				view_remove_from_parent,
				view_set_cursor_id,
				control_set_device,
				button_show,
				dirty_button_draw_children,
				view_move_children,
			},
			button_set_value,
			control_on_command,
			control_focus_sibling,
			button_on_key_down,
			control_set_focus,
			button_create_or_destroy,
		},
		button_on_char,
		button_on_button_down,
		button_on_button_up,
		button_on_mouse_move,
	},
	dirty_button_set_dirty,
};

void DirtyButtonInit(struct dirty_button *self, int32_t id, const struct event *event, const struct rect *rect,
	struct cstr *label, int32_t value, int32_t font)
{
	ButtonInit(&self->base, id, event, rect, label, 1, value, font);
	self->base.base.base.base.base.ops = &DIRTY_BUTTON_OPS_TABLE.base.base.base.base.base;

	self->dirty = false;
}

struct dirty_button *DirtyButtonCreate(int32_t id, const struct event *event, const struct rect *rect,
	struct cstr *label, int32_t value, int32_t font)
{
	struct dirty_button *self = MemAlloc(1, sizeof(struct dirty_button));
	DirtyButtonInit(self, id, event, rect, label, value, font);

	return self;
}

void DirtyButtonDestroy(struct dirty_button **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void dirty_button_destroy(struct object *self)
{
	self->ops = &BUTTON_OPS_TABLE.base.base.base.base;
	button_destroy(self);
}

void dirty_button_draw_children(struct view *view, struct screen_dc *dc, const struct rect *clip_rect)
{
	struct dirty_button *self = (struct dirty_button *) view;

	if (self->dirty) {
		CONTROL_OPS(self)->SetValue(&self->base.base, 1, false);
		self->dirty = false;
	}

	button_draw_children(view, dc, clip_rect);
}

void dirty_button_set_dirty(struct dirty_button *self)
{
	self->dirty = true;
}


const struct text_btn_ops TEXT_BTN_OPS_TABLE = {
	{
		{
			{
				{
					{text_btn_destroy},
					evt_handler_is_handled,
					control_set_handled_for,
					control_set_handled,
					control_clear_handled_for,
					control_clear_handled,
					text_btn_on_event,
					view_on_idle,
					text_btn_on_idle_spare,
				},
				view_add_child,
				button_get_bounds,
				view_get_clip_region,
				button_hide,
				view_remove_child,
				view_remove_from_parent,
				view_set_cursor_id,
				control_set_device,
				button_show,
				button_draw_children,
				view_move_children,
			},
			button_set_value,
			control_on_command,
			control_focus_sibling,
			button_on_key_down,
			control_set_focus,
			button_create_or_destroy,
		},
		button_on_char,
		button_on_button_down,
		button_on_button_up,
		button_on_mouse_move,
	},
};

void TextBtnInit(struct text_btn *self, int32_t id, const struct event *event, const struct rect *rect,
	struct cstr *label, int32_t value, int32_t font)
{
	ButtonInit(&self->base, id, event, rect, label, 3, value, font);
	self->base.base.base.base.base.ops = &TEXT_BTN_OPS_TABLE.base.base.base.base.base;
}

struct text_btn *TextBtnCreate(int32_t id, const struct event *event, const struct rect *rect, struct cstr *label,
	int32_t value, int32_t font)
{
	struct text_btn *self = MemAlloc(1, sizeof(struct text_btn));
	TextBtnInit(self, id, event, rect, label, value, font);

	return self;
}

void TextBtnDestroy(struct text_btn **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void text_btn_destroy(struct object *self)
{
	self->ops = &BUTTON_OPS_TABLE.base.base.base.base;
	button_destroy(self);
}

struct view *CtrlFactoryTextBtn(struct ctrl_desc *desc)
{
	struct rect rect = desc_rect(desc);
	struct cstr *label = desc_label(desc);

	struct text_btn *button = TextBtnCreate(desc->id, desc->event, &rect, label, desc->value, 0);
	ControlApplyDescriptor(&button->base.base, desc);

	return &button->base.base.base;
}

void text_btn_on_event(struct evt_handler *handler, struct cmd_event *event)
{
	struct text_btn *self = (struct text_btn *) handler;

	button_on_event(handler, event);

	if (event->command == CMD_MOUSE) {
		struct mouse_event *mouse = (struct mouse_event *) event;

		if (mouse->button_down) {
			struct point pt = mouse->mouse_pos;

			if (ViewPointInBounds(&self->base.base.base, &pt))
				EvtHandlerAddIdleHandler(handler, false, 4);
		}

	} else {
		if (event->command == CMD_BUTTON_HILITE) {
			if (self->base.base.slider_value) {
				EvtHandlerAddIdleHandler(handler, false, 4);

			} else {
				EvtHandlerRemoveIdleHandler(handler, false);
			}
		}
	}
}

void text_btn_on_idle_spare(struct evt_handler *handler, void *task)
{
	(void) task;
	struct text_btn *self = (struct text_btn *) handler;

	if (self->base.base.slider_value) {
		struct control *ctrl = &self->base.base;
		struct cmd_event *cmd = CmdEventCreateSpecsFlag(ctrl->command_id, &handler->eh_event, &ctrl->c_event, false);
		cmd->param1 = ctrl->command_p1;
		cmd->param2 = ctrl->command_p2;
		DispatchGlobalEvent(cmd);
	}
}
