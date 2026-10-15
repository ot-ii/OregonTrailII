#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "control.h"
#include "../app/event.h"
#include "../base/geom.h"

struct button;
struct cstr;
struct dirty_button;
struct disp_obj;
struct screen_dc;
struct window;

struct button_ops {
	struct control_ops base;
	intptr_t (*OnChar)(struct button *self, void *hwnd, int32_t key, int32_t lparam_lo);
	void (*OnButtonDown)(struct button *self, void *hwnd, int32_t dblclk, int32_t lparam_lo, int32_t lparam_hi,
		uint32_t wparam);
	void (*OnButtonUp)(struct button *self, void *hwnd, int32_t lparam_lo, int32_t lparam_hi, uint32_t wparam);
	void (*OnMouseMove)(struct button *self, void *hwnd, int32_t lparam_lo, int32_t lparam_hi, uint32_t wparam);
};

struct button {
	struct control base;
	struct disp_obj *data_src;
	struct cstr *label_rsrc;
	int32_t font_id;
	bool is_toggle;
};

#define BUTTON_OPS(p) ((const struct button_ops *) ((const struct object *) (p))->ops)

extern const struct button_ops BUTTON_OPS_TABLE;

void ButtonInitDispObj(struct button *self, int32_t id, const struct event *event, struct disp_obj *data_obj,
	const struct rect *rect, int32_t ctrl_type, int32_t init_value);
void ButtonInit(struct button *self, int32_t id, const struct event *event, const struct rect *rect,
	struct cstr *label, int32_t ctrl_type, int32_t value, int32_t font);
void ButtonInitPoint(struct button *self, int32_t id, const struct event *event_src, const struct point *pos,
	struct cstr *label, int32_t ctrl_type, int32_t value, int32_t font);
void ButtonInitDispObjPoint(struct button *self, int32_t id, const struct event *event_src, struct disp_obj *rect_src,
	const struct point *pos, int32_t control_type, int32_t value);
struct button *ButtonCreateDispObj(int32_t id, const struct event *event, struct disp_obj *data_obj,
	const struct rect *rect, int32_t ctrl_type, int32_t init_value);
struct button *ButtonCreate(int32_t id, const struct event *event, const struct rect *rect, struct cstr *label,
	int32_t ctrl_type, int32_t value, int32_t font);
struct button *ButtonCreatePoint(int32_t id, const struct event *event_src, const struct point *pos,
	struct cstr *label, int32_t ctrl_type, int32_t value, int32_t font);
struct button *ButtonCreateDispObjPoint(int32_t id, const struct event *event_src, struct disp_obj *rect_src,
	const struct point *pos, int32_t control_type, int32_t value);
void ButtonDestroy(struct button **ctx_mod);
void button_destroy(struct object *self);

void button_on_event(struct evt_handler *self, struct cmd_event *event);
struct rect button_get_bounds(struct view *self);
void button_hide(struct view *self, bool fill, bool invalidate);
void button_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds);
void button_show(struct view *self);
void button_set_value(struct control *self, int32_t checked, bool notify);
void button_on_key_down(struct control *self, void *hwnd, uint32_t wparam, int32_t reserved, int32_t lparam_lo,
	uint32_t lparam_hi);
void button_create_or_destroy(struct control *self, struct window *parent);
intptr_t button_on_char(struct button *self, void *hwnd, int32_t key, int32_t lparam_lo);
void button_on_button_down(struct button *self, void *hwnd, int32_t dblclk, int32_t lparam_lo, int32_t lparam_hi,
	uint32_t wparam);
void button_on_button_up(struct button *self, void *hwnd, int32_t lparam_lo, int32_t lparam_hi, uint32_t wparam);
void button_on_mouse_move(struct button *self, void *hwnd, int32_t lparam_lo, int32_t lparam_hi, uint32_t wparam);

void ButtonInitData(struct button *self, struct disp_obj *arg_data_src, int32_t value, int32_t style);
void ButtonInitButton(struct button *self, struct cstr *label, int32_t value, int32_t style, int32_t font);
void ButtonSetData(struct button *self, struct cstr *data, bool redraw);
intptr_t ButtonDispatchWndMsg(struct button *self, void *hwnd, uint32_t msg, uintptr_t wparam, intptr_t lparam);

struct view *CreateControlFromTemplate(struct ctrl_desc *desc);


struct check_box_ops {
	struct button_ops base;
};

struct check_box {
	struct button base;
};

#define CHECK_BOX_OPS(p) ((const struct check_box_ops *) ((const struct object *) (p))->ops)

extern const struct check_box_ops CHECK_BOX_OPS_TABLE;

void CheckBoxInit(struct check_box *self, int32_t id, const struct event *event, const struct rect *rect,
	struct cstr *label, int32_t value, int32_t font);
struct check_box *CheckBoxCreate(int32_t id, const struct event *event, const struct rect *rect, struct cstr *label,
	int32_t value, int32_t font);
void CheckBoxDestroy(struct check_box **ctx_mod);
void check_box_destroy(struct object *self);


struct clip_button_ops {
	struct button_ops base;
};

struct clip_button {
	struct button base;
};

#define CLIP_BUTTON_OPS(p) ((const struct clip_button_ops *) ((const struct object *) (p))->ops)

extern const struct clip_button_ops CLIP_BUTTON_OPS_TABLE;

void ClipButtonInit(struct clip_button *self, int32_t id, const struct event *event, const struct rect *rect,
	int32_t command_id, int32_t command_p1, int32_t command_p2);
struct clip_button *ClipButtonCreate(int32_t id, const struct event *event, const struct rect *rect,
	int32_t command_id, int32_t command_p1, int32_t command_p2);
void ClipButtonDestroy(struct clip_button **ctx_mod);
void clip_button_destroy(struct object *self);

struct view *CtrlCreateFromDesc(struct ctrl_desc *desc);

struct def_button_ops {
	struct button_ops base;
};

struct def_button {
	struct button base;
};

#define DEF_BUTTON_OPS(p) ((const struct def_button_ops *) ((const struct object *) (p))->ops)

extern const struct def_button_ops DEF_BUTTON_OPS_TABLE;

void DefButtonInit(struct def_button *self, int32_t id, const struct event *event, const struct rect *rect,
	struct cstr *label, int32_t value, int32_t font);
struct def_button *DefButtonCreate(int32_t id, const struct event *event, const struct rect *rect,
	struct cstr *label, int32_t value, int32_t font);
void DefButtonDestroy(struct def_button **ctx_mod);
void def_button_destroy(struct object *self);

bool DefBtnKeyFilter(const char *in_key, char *out_key);


struct dirty_button_ops {
	struct button_ops base;
	void (*SetDirty)(struct dirty_button *self);
};

struct dirty_button {
	struct button base;
	bool dirty;
};

#define DIRTY_BUTTON_OPS(p) ((const struct dirty_button_ops *) ((const struct object *) (p))->ops)

extern const struct dirty_button_ops DIRTY_BUTTON_OPS_TABLE;

void DirtyButtonInit(struct dirty_button *self, int32_t id, const struct event *event, const struct rect *rect,
	struct cstr *label, int32_t value, int32_t font);
struct dirty_button *DirtyButtonCreate(int32_t id, const struct event *event, const struct rect *rect,
	struct cstr *label, int32_t value, int32_t font);
void DirtyButtonDestroy(struct dirty_button **ctx_mod);
void dirty_button_destroy(struct object *self);

void dirty_button_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds);
void dirty_button_set_dirty(struct dirty_button *self);


struct text_btn_ops {
	struct button_ops base;
};

struct text_btn {
	struct button base;
};

#define TEXT_BTN_OPS(p) ((const struct text_btn_ops *) ((const struct object *) (p))->ops)

extern const struct text_btn_ops TEXT_BTN_OPS_TABLE;

void TextBtnInit(struct text_btn *self, int32_t id, const struct event *event, const struct rect *rect,
	struct cstr *label, int32_t value, int32_t font);
struct text_btn *TextBtnCreate(int32_t id, const struct event *event, const struct rect *rect, struct cstr *label,
	int32_t value, int32_t font);
void TextBtnDestroy(struct text_btn **ctx_mod);
void text_btn_destroy(struct object *self);

void text_btn_on_event(struct evt_handler *self, struct cmd_event *event);
void text_btn_on_idle_spare(struct evt_handler *self, void *task);

struct view *CtrlFactoryTextBtn(struct ctrl_desc *desc);
