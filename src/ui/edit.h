#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "control.h"
#include "../app/event.h"
#include "../base/geom.h"

struct cstr;
struct screen_dc;
struct window;

struct edit_ops {
	struct control_ops base;
};

struct edit {
	struct control base;
	struct cstr *text;
	int32_t font_id;
	int32_t bk_color;
	int32_t pen_color;
	int32_t text_color;
	bool hscroll;
	bool sel_on_focus;
	bool inert;
	int32_t vscroll;
	int32_t max_len;
	int32_t input_filter;
	bool prefer_multiline;
	bool change_pending;
};

#define EDIT_OPS(p) ((const struct edit_ops *) ((const struct object *) (p))->ops)

extern const struct edit_ops EDIT_OPS_TABLE;

void EditInit(struct edit *self, int32_t id, const struct event *event, const struct rect *rect,
	struct cstr *init_text, bool wants_enter, bool hscroll, int32_t vscroll, int32_t font_id, int32_t max_len,
	bool sel_on_focus, bool inert, int32_t input_filter, bool prefer_multiline);
struct edit *EditCreate(int32_t id, const struct event *event, const struct rect *rect, struct cstr *init_text,
	bool wants_enter, bool hscroll, int32_t vscroll, int32_t font_id, int32_t max_len, bool sel_on_focus, bool inert,
	int32_t input_filter, bool prefer_multiline);
void EditDestroy(struct edit **ctx_mod);
void edit_destroy(struct object *self);

void edit_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds);
void edit_set_focus(struct control *self, bool gaining);
void edit_create_or_destroy(struct control *self, struct window *parent);
void edit_on_event(struct evt_handler *self, struct cmd_event *event);
void edit_on_key_down(struct control *self, void *hwnd, uint32_t wparam, int32_t reserved, int32_t lparam_lo,
	uint32_t lparam_hi);

void EditSetText(struct edit *self, struct cstr *text);
intptr_t EditDispatchMessage(struct edit *self, void *hwnd, uint32_t msg, uintptr_t wparam, intptr_t lparam);
void EditOnChar(struct edit *self, void *hwnd, uint32_t c, int32_t x);
int32_t EditOnButton(struct edit *self, void *hwnd, int32_t dblclk, int32_t x, int32_t y, uintptr_t wparam);
int32_t EditOnKillFocus(struct edit *self, void *hwnd, uintptr_t wparam);
void EditSetSel(struct edit *self, uint32_t start, uint32_t end);
struct cstr *EditGetText(struct edit *self);
bool EditValidateInput(struct edit *self, uint32_t c, bool is_paste, int32_t filter);
bool EditIsDigit(struct edit *self, uint32_t c);
void EditSetSelOnFocus(struct edit *self, bool val);

struct view *CtrlFactoryEdit(struct ctrl_desc *desc);
