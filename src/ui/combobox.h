#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "control.h"
#include "../app/event.h"
#include "../base/geom.h"

struct cstr;
struct screen_dc;
struct strrsrc;
struct window;

struct combo_box_ops {
	struct control_ops base;
};

struct combo_box {
	struct control base;
	struct strrsrc *items;
	int32_t font_id;
	int32_t field_88;
	int32_t field_8C;
	int32_t field_90;
	int32_t drop_height_extra;
	bool disabled;
};

#define COMBO_BOX_OPS(p) ((const struct combo_box_ops *) ((const struct object *) (p))->ops)

extern const struct combo_box_ops COMBO_BOX_OPS_TABLE;

void ComboBoxInit(struct combo_box *self, int32_t id, const struct event *event, const struct rect *rect,
	int32_t field94val, struct strrsrc *init_items, int32_t val);
struct combo_box *ComboBoxCreate(int32_t id, const struct event *event, const struct rect *rect, int32_t field94val,
	struct strrsrc *init_items, int32_t val);
void ComboBoxDestroy(struct combo_box **ctx_mod);
void combo_box_destroy(struct object *self);

void combo_box_on_event(struct evt_handler *self, struct cmd_event *event);
void combo_box_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds);
void combo_box_set_value(struct control *self, int32_t value, bool notify);
void combo_box_on_command(struct control *self, void *parent_hwnd, int32_t id, void *ctrl_hwnd, int32_t notify);
void combo_box_create_or_destroy(struct control *self, struct window *parent);

void ComboBoxAddItem(struct combo_box *self, struct cstr *item);
void ComboBoxSetList(struct combo_box *self, struct strrsrc *new_items, int32_t value);
void ComboBoxSetEnabled(struct combo_box *self, bool enabled);
intptr_t ComboBoxHandleKeydown(struct combo_box *self, void *hwnd, uint32_t msg, uintptr_t wparam, intptr_t lparam);

struct view *ComboCreateFromDesc(struct ctrl_desc *desc);
