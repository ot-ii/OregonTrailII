#pragma once

#include <stdint.h>

#include "control.h"
#include "../app/event.h"
#include "../base/geom.h"

struct cstr;
struct screen_dc;
struct strrsrc;
struct window;

struct list_box_ops {
	struct control_ops base;
};

struct list_box {
	struct control base;
	struct strrsrc *items;
	int32_t sel_index;
	int32_t unused_88;
	int32_t unused_8A;
	int32_t bk_color;
	int32_t pen_color;
	int32_t text_color;
};

#define LIST_BOX_OPS(p) ((const struct list_box_ops *) ((const struct object *) (p))->ops)

extern const struct list_box_ops LIST_BOX_OPS_TABLE;

void ListBoxInit(struct list_box *self, int32_t id, const struct event *event, const struct rect *rect,
	struct strrsrc *init_items, int32_t sel);
struct list_box *ListBoxCreate(int32_t id, const struct event *event, const struct rect *rect,
	struct strrsrc *init_items, int32_t sel);
void ListBoxDestroy(struct list_box **ctx_mod);
void list_box_destroy(struct object *self);

void list_box_on_event(struct evt_handler *self, struct cmd_event *event);
void list_box_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds);
void list_box_create_or_destroy(struct control *self, struct window *parent);
void list_box_on_command(struct control *self, void *parent_hwnd, int32_t id, void *ctrl_hwnd, int32_t notify);

void ListBoxReset(struct list_box *self);
void ListBoxSetSourceById(struct list_box *self, int32_t id);
void ListBoxSetSel(struct list_box *self, int32_t index);
void ListBoxAddItem(struct list_box *self, struct cstr *item);
void ListBoxSetSource(struct list_box *self, struct strrsrc *new_items);

struct view *CtrlFactoryListBox(struct ctrl_desc *desc);
