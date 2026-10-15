#pragma once

#include <stdint.h>

#include "../base/geom.h"
#include "textlabel.h"

struct cstr;
struct drawable;
struct screen_dc;
struct stateful_disp_obj;

struct stateful_disp_obj_ops {
	struct text_label_ops base;
	int32_t (*DrawSource)(struct stateful_disp_obj *self, struct screen_dc *dc, struct point *pos, int32_t index);
	void (*ApplyMargins)(struct stateful_disp_obj *self);
};

struct stateful_disp_obj {
	struct text_label base;
	struct rect text_rect0;
	struct rect text_rect1;
	struct rect source_rect;
	struct drawable *source_obj;
	int32_t num_states;
	struct point margin;
};

#define STATEFUL_DISP_OBJ_OPS(p) ((const struct stateful_disp_obj_ops *) ((const struct object *) (p))->ops)

extern const struct stateful_disp_obj_ops STATEFUL_DISP_OBJ_OPS_TABLE;

void StatefulDispObjInit(struct stateful_disp_obj *self, int32_t type_id, struct cstr *source, struct point *pos,
	int32_t font_id, int32_t margin_x, int32_t margin_y);
struct stateful_disp_obj *StatefulDispObjCreate(int32_t type_id, struct cstr *source, struct point *pos,
	int32_t font_id, int32_t margin_x, int32_t margin_y);
void StatefulDispObjDestroy(struct stateful_disp_obj **ctx_mod);
void stateful_disp_obj_destroy(struct object *self);

void stateful_disp_obj_draw_inner(struct text_label *self, struct screen_dc *dc, struct point *offset,
	int32_t value, int32_t draw_mode, int32_t a6);

int32_t stateful_disp_obj_draw_source(struct stateful_disp_obj *self, struct screen_dc *dc, struct point *pos,
	int32_t index);
void stateful_disp_obj_apply_margins(struct stateful_disp_obj *self);
