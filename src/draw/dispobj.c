#include "dispobj.h"

#include <stddef.h>

#include "label.h"
#include "../base/memory.h"
#include "region.h"
#include "shape.h"
#include "text.h"

const struct disp_obj_ops DISP_OBJ_OPS_TABLE = {
	{
		{
			{disp_obj_destroy},
			text_label_get_bounds,
			text_label_set_bounds,
			stateful_disp_obj_draw_inner,
			disp_obj_make_region,
			text_label_set_text,
			text_label_draw,
		},
		stateful_disp_obj_draw_source,
		disp_obj_apply_margins,
	},
};

void DispObjInit(struct disp_obj *self, int32_t type_id, struct cstr *source, struct rect *rect, int32_t font_id,
	int32_t margin_x, int32_t margin_y)
{
	struct point top_left = PointMake(rect->left, rect->top);
	StatefulDispObjInit(&self->base, type_id, source, &top_left, font_id, margin_x, margin_y);
	self->base.base.base.ops = &DISP_OBJ_OPS_TABLE.base.base.base;

	struct stateful_disp_obj *sdo = &self->base;
	struct text_label *tl = &sdo->base;

	sdo->margin.v = TEXT_LABEL_OPS(self)->GetBounds(tl).right + margin_x;
	STATEFUL_DISP_OBJ_OPS(self)->ApplyMargins(sdo);
	sdo->margin.h = TEXT_LABEL_OPS(self)->GetBounds(tl).top + margin_y;
	STATEFUL_DISP_OBJ_OPS(self)->ApplyMargins(sdo);

	if (tl->text) {
		tl->text->base.h_align = 0;
		tl->text->base.v_align = 0;
	}

	tl->bounds = *rect;

	if (tl->text)
		SHAPE_OPS(tl->text)->SetRect(&tl->text->base.base, rect);

	STATEFUL_DISP_OBJ_OPS(self)->ApplyMargins(sdo);
}

struct disp_obj *DispObjCreate(int32_t type_id, struct cstr *source, struct rect *rect, int32_t font_id,
	int32_t margin_x, int32_t margin_y)
{
	struct disp_obj *ctx = MemAlloc(1, sizeof(struct disp_obj));
	DispObjInit(ctx, type_id, source, rect, font_id, margin_x, margin_y);

	return ctx;
}

void DispObjDestroy(struct disp_obj **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void disp_obj_destroy(struct object *self)
{
	self->ops = &STATEFUL_DISP_OBJ_OPS_TABLE.base.base;
	stateful_disp_obj_destroy(self);
}

struct region *disp_obj_make_region(struct text_label *self, struct point *offset)
{
	struct stateful_disp_obj *sdo = (struct stateful_disp_obj *) self;
	struct rect region_rect = sdo->source_rect;
	region_rect.left = region_rect.left + offset->h;
	region_rect.right = region_rect.right + offset->h;
	region_rect.top = region_rect.top + offset->v;
	region_rect.bottom = region_rect.bottom + offset->v;

	return RegionCreateRect(&region_rect, 1, 0, 0);
}

void disp_obj_apply_margins(struct stateful_disp_obj *self)
{
	self->text_rect0 = TEXT_LABEL_OPS(self)->GetBounds(&self->base);
	self->text_rect0.left = self->text_rect0.left + self->margin.v;
	self->text_rect0.top = self->text_rect0.top + self->margin.h;
	self->text_rect1 = self->text_rect0;
}
