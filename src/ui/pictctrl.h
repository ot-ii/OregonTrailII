#pragma once

#include <stdint.h>

#include "button.h"
#include "../app/event.h"
#include "../base/geom.h"

struct canvas;
struct cstr;
struct screen_dc;

struct pict_ctrl_ops {
	struct button_ops base;
};

struct pict_ctrl {
	struct button base;
};

#define PICT_CTRL_OPS(p) ((const struct pict_ctrl_ops *) ((const struct object *) (p))->ops)

extern const struct pict_ctrl_ops PICT_CTRL_OPS_TABLE;

void PictCtrlInit(struct pict_ctrl *self, int32_t id, const struct event *event_src, const struct point *pos,
	int32_t obj_type, int32_t style, int32_t value, struct cstr *source, int32_t font_id, int32_t margin_x,
	int32_t margin_y);
struct pict_ctrl *PictCtrlCreate(int32_t id, const struct event *event_src, const struct point *pos,
	int32_t obj_type, int32_t style, int32_t value, struct cstr *source, int32_t font_id, int32_t margin_x,
	int32_t margin_y);
void PictCtrlDestroy(struct pict_ctrl **ctx_mod);
void pict_ctrl_destroy(struct object *self);

struct view *CtrlFactoryPicControl(struct ctrl_desc *desc);

struct pict_check_ops {
	struct pict_ctrl_ops base;
};

struct pict_check {
	struct pict_ctrl base;
};

#define PICT_CHECK_OPS(p) ((const struct pict_check_ops *) ((const struct object *) (p))->ops)

extern const struct pict_check_ops PICT_CHECK_OPS_TABLE;

void PictCheckInit(struct pict_check *self, int32_t id, const struct event *event_src, const struct point *pos,
	int32_t obj_type, int32_t value, struct cstr *source, int32_t font_id);
struct pict_check *PictCheckCreate(int32_t id, const struct event *event_src, const struct point *pos,
	int32_t obj_type, int32_t value, struct cstr *source, int32_t font_id);
void PictCheckDestroy(struct pict_check **ctx_mod);
void pict_check_destroy(struct object *self);


struct pict_radio_ops {
	struct pict_ctrl_ops base;
};

struct pict_radio {
	struct pict_ctrl base;
};

#define PICT_RADIO_OPS(p) ((const struct pict_radio_ops *) ((const struct object *) (p))->ops)

extern const struct pict_radio_ops PICT_RADIO_OPS_TABLE;

void PictRadioInit(struct pict_radio *self, int32_t id, const struct event *event_src, const struct point *pos,
	int32_t obj_type, int32_t value, struct cstr *source, int32_t font_id);
struct pict_radio *PictRadioCreate(int32_t id, const struct event *event_src, const struct point *pos,
	int32_t obj_type, int32_t value, struct cstr *source, int32_t font_id);
void PictRadioDestroy(struct pict_radio **ctx_mod);
void pict_radio_destroy(struct object *self);


struct scroll_ctrl_ops {
	struct pict_ctrl_ops base;
};

struct scroll_ctrl {
	struct pict_ctrl base;
	struct canvas *scrollbar;
};

#define SCROLL_CTRL_OPS(p) ((const struct scroll_ctrl_ops *) ((const struct object *) (p))->ops)

extern const struct scroll_ctrl_ops SCROLL_CTRL_OPS_TABLE;

void ScrollCtrlInit(struct scroll_ctrl *self, int32_t type, const struct event *event_src, const struct point *pos,
	int32_t obj_type, struct canvas *scrollbar);
struct scroll_ctrl *ScrollCtrlCreate(int32_t type, const struct event *event_src, const struct point *pos,
	int32_t obj_type, struct canvas *scrollbar);
void ScrollCtrlDestroy(struct scroll_ctrl **ctx_mod);
void scroll_ctrl_destroy(struct object *self);

void scroll_ctrl_draw_children(struct view *self, struct screen_dc *dc, const struct rect *invalid_rect);
