#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../app/event.h"
#include "../base/geom.h"
#include "view.h"

struct control;
struct screen_dc;
struct window;

enum control_type {
	CTRL_PUSH = 0,
	CTRL_RADIO = 1,
	CTRL_CHECKBOX = 2,
	CTRL_TEXT_BUTTON = 3,
	CTRL_HOTSPOT = 4,
	CTRL_EDIT = 5,
	CTRL_SCROLLBAR = 6,
	CTRL_LISTBOX = 7,
	CTRL_COMBOBOX = 8,
	CTRL_MCI_VIDEO = 9,
};

struct control_ops {
	struct view_ops base;
	void (*SetValue)(struct control *self, int32_t value, bool notify);
	void (*OnCommand)(struct control *self, void *parent_hwnd, int32_t id, void *ctrl_hwnd, int32_t notify);
	void (*FocusSibling)(struct control *self, void *hwnd, bool prev);
	void (*OnKeyDown)(struct control *self, void *hwnd, uint32_t wparam, int32_t reserved, int32_t lparam_lo,
		uint32_t lparam_hi);
	void (*SetFocus)(struct control *self, bool gaining);
	void (*CreateOrDestroy)(struct control *self, struct window *parent);
};

struct control {
	struct view base;
	struct event c_event;
	int32_t command_id;
	int32_t command_p1;
	int32_t command_p2;
	int32_t slider_value;
	int32_t slider_min;
	int32_t slider_max;
	int32_t value;
	bool is_default;
	bool wants_enter;
	bool owns_window;
	int32_t win_style;
	void *hwnd;
	void *orig_wndproc;
	int32_t control_type;
	int32_t flags;
	int32_t focus_order;
	bool redraw_pending;
};

#define CONTROL_OPS(p) ((const struct control_ops *) ((const struct object *) (p))->ops)

extern const struct control_ops CONTROL_OPS_TABLE;

void ControlInit(struct control *self, int32_t control_type, int32_t id, const struct event *event,
	const struct rect *rect, int32_t flags, int32_t focus_order);
void ControlInitPoint(struct control *self, int32_t control_type, int32_t id, const struct event *event_src,
	const struct point *pos, int32_t flags, int32_t focus_order);
struct control *ControlCreate(int32_t control_type, int32_t id, const struct event *event, const struct rect *rect,
	int32_t flags, int32_t focus_order);
struct control *ControlCreatePoint(int32_t control_type, int32_t id, const struct event *event_src,
	const struct point *pos, int32_t flags, int32_t focus_order);
void ControlDestroy(struct control **ctx_mod);
void control_destroy(struct object *self);

void control_set_handled_for(struct evt_handler *self, struct event *event);
void control_set_handled(struct evt_handler *self);
void control_clear_handled_for(struct evt_handler *self, struct event *event);
void control_clear_handled(struct evt_handler *self);
void control_on_event(struct evt_handler *self, struct cmd_event *event);
void control_hide(struct view *self, bool fill, bool invalidate);
void control_set_device(struct view *self, struct window *window);
void control_show(struct view *self);
void control_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds);
void control_set_value(struct control *self, int32_t value, bool notify);
void control_on_command(struct control *self, void *parent_hwnd, int32_t id, void *ctrl_hwnd, int32_t notify);
void control_focus_sibling(struct control *self, void *hwnd, bool prev);
void control_on_key_down(struct control *self, void *hwnd, uint32_t wparam, int32_t reserved, int32_t lparam_lo,
	uint32_t lparam_hi);
void control_set_focus(struct control *self, bool gaining);
void control_create_or_destroy(struct control *self, struct window *parent);

void ControlGetDims(struct control *self, int32_t *out_id, int32_t *out_p1, int32_t *out_p2);
void ControlFireCommand(struct control *self);
void ControlSetSliderValue(struct control *self, int32_t value);
void ControlSetSliderMax(struct control *self, int32_t max);
void ControlSetSliderMin(struct control *self, int32_t min);
void ControlSetCommand(struct control *self, int32_t id, int32_t command_p1, int32_t command_p2);
void ControlInitFields(struct control *self);
int32_t ControlGetFocusIndex(struct control *self);
void ControlSetFocusOrder(struct control *self, int32_t focus_order);
void ControlRelinquishFocus(struct control *self);
intptr_t ControlCallDefaultWndProc(struct control *self, void *hwnd, uint32_t msg, uintptr_t wparam, intptr_t lparam);
void ControlApplyDescriptor(struct control *self, struct ctrl_desc *descriptor);
