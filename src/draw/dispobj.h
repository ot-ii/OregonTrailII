#pragma once

#include <stdint.h>

#include "../base/geom.h"
#include "statefuldispobj.h"

struct cstr;
struct region;

struct disp_obj_ops {
	struct stateful_disp_obj_ops base;
};

struct disp_obj {
	struct stateful_disp_obj base;
};

#define DISP_OBJ_OPS(p) ((const struct disp_obj_ops *) ((const struct object *) (p))->ops)

extern const struct disp_obj_ops DISP_OBJ_OPS_TABLE;

void DispObjInit(struct disp_obj *self, int32_t type_id, struct cstr *source, struct rect *rect, int32_t font_id,
	int32_t margin_x, int32_t margin_y);
struct disp_obj *DispObjCreate(int32_t type_id, struct cstr *source, struct rect *rect, int32_t font_id,
	int32_t margin_x, int32_t margin_y);
void DispObjDestroy(struct disp_obj **ctx_mod);
void disp_obj_destroy(struct object *self);

struct region *disp_obj_make_region(struct text_label *self, struct point *offset);
void disp_obj_apply_margins(struct stateful_disp_obj *self);
