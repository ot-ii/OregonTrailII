#include "statefuldispobj.h"

#include <stddef.h>

#include "drawable.h"
#include "mediabase.h"
#include "../base/memory.h"
#include "pictserver.h"
#include "screendc.h"
#include "server.h"
#include "shape.h"
#include "text.h"

const struct stateful_disp_obj_ops STATEFUL_DISP_OBJ_OPS_TABLE = {
	{
		{stateful_disp_obj_destroy},
		text_label_get_bounds,
		text_label_set_bounds,
		stateful_disp_obj_draw_inner,
		text_label_make_region,
		text_label_set_text,
		text_label_draw,
	},
	stateful_disp_obj_draw_source,
	stateful_disp_obj_apply_margins,
};

void StatefulDispObjInit(struct stateful_disp_obj *self, int32_t type_id, struct cstr *source, struct point *pos,
	int32_t font_id, int32_t margin_x, int32_t margin_y)
{
	TextLabelInit(&self->base, source, font_id, 0);
	self->base.base.ops = &STATEFUL_DISP_OBJ_OPS_TABLE.base.base;
	self->text_rect0 = RectMake(0, 0, 0, 0);
	self->text_rect1 = RectMake(0, 0, 0, 0);
	self->source_rect = RectMake(0, 0, 0, 0);

	struct point size = PointMake(0, 0);

	self->margin.v = margin_x;
	self->margin.h = margin_y;
	self->num_states = 0;
	self->source_obj = ServerCreateVirtual(&g_pict_server->base.base, type_id, true);

	if (self->source_obj) {
		self->num_states = self->source_obj->item_count;
		DRAWABLE_OPS(self->source_obj)->GetSizeAt(self->source_obj, &size, 0);
	}

	self->source_rect.left = pos->h;
	self->source_rect.right = pos->h + size.h;
	self->source_rect.top = pos->v;
	self->source_rect.bottom = pos->v + size.v;

	self->base.bounds = self->source_rect;

	if (self->base.text)
		SHAPE_OPS(self->base.text)->SetRect(&self->base.text->base.base, &self->source_rect);

	stateful_disp_obj_apply_margins(self);
}

struct stateful_disp_obj *StatefulDispObjCreate(int32_t type_id, struct cstr *source, struct point *pos,
	int32_t font_id, int32_t margin_x, int32_t margin_y)
{
	struct stateful_disp_obj *ctx = MemAlloc(1, sizeof(struct stateful_disp_obj));
	StatefulDispObjInit(ctx, type_id, source, pos, font_id, margin_x, margin_y);

	return ctx;
}

void StatefulDispObjDestroy(struct stateful_disp_obj **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void stateful_disp_obj_destroy(struct object *self)
{
	struct stateful_disp_obj *sdo = (struct stateful_disp_obj *) self;

	if (sdo->source_obj)
		ReleaseDrawable(&sdo->source_obj->base.base, false);

	self->ops = &TEXT_LABEL_OPS_TABLE.base;
	text_label_destroy(self);
}

void stateful_disp_obj_draw_inner(struct text_label *self, struct screen_dc *dc, struct point *offset,
	int32_t value, int32_t draw_mode, int32_t a6)
{
	(void) a6;

	struct stateful_disp_obj *sdo = (struct stateful_disp_obj *) self;
	int32_t max_state = sdo->num_states - 1;
	int32_t state = 0;
	int32_t draw_state = 0;
	struct rect *text_rect;
	struct point origin;

	SCREEN_DC_OPS(dc)->SetColor(dc, 0);
	SCREEN_DC_OPS(dc)->SetBkColor(dc, 255);

	struct rect bounds_v = TEXT_LABEL_OPS(self)->GetBounds(self);
	struct rect bounds_h = TEXT_LABEL_OPS(self)->GetBounds(self);
	origin.h = bounds_h.left + offset->h;
	origin.v = offset->v + bounds_v.top;

	if (draw_mode == 2) {
		draw_state = 1;

	} else if (draw_mode == 1) {
		draw_state = 2;
	}

	switch (max_state) {
		case 0:
			state = 0;
			text_rect = &sdo->text_rect0;
			break;
		case 1:
			if (value > 0 || draw_state == 1) {
				state = 1;

			} else {
				state = 0;
			}

			text_rect = state == 1 ? &sdo->text_rect1 : &sdo->text_rect0;
			break;
		case 2:
			if (value > 0) {
				state = 1;

			} else {
				state = draw_state;
			}

			text_rect = state == 1 ? &sdo->text_rect1 : &sdo->text_rect0;
			break;
		case 3:
			state = (draw_state == 1) + 2 * value;
			text_rect = state % 2 == 1 ? &sdo->text_rect1 : &sdo->text_rect0;

			if (state > max_state)
				state = state % 2 + 2 * (max_state / 2);

			break;
		default:
			state = draw_state + 3 * value;
			text_rect = draw_state == 1 ? &sdo->text_rect1 : &sdo->text_rect0;

			if (state > max_state)
				state = state % 3 + 3 * (max_state / 3);

			break;
	}

	STATEFUL_DISP_OBJ_OPS(self)->DrawSource(sdo, dc, &origin, state);

	if (self->text)
		TEXT_LABEL_OPS(self)->Draw(self, dc, offset, text_rect, self->align != 0, draw_state);
}

void stateful_disp_obj_apply_margins(struct stateful_disp_obj *self)
{
	self->text_rect1 = TEXT_LABEL_OPS(self)->GetBounds(&self->base);
	self->text_rect0 = self->text_rect1;

	if (self->margin.v > 0) {
		self->text_rect0.right = self->text_rect0.right - self->margin.v;
		self->text_rect1.left = self->text_rect1.left + self->margin.v;

	} else if (self->margin.v < 0) {
		self->text_rect0.left = self->text_rect0.left - self->margin.v;
		self->text_rect1.right = self->text_rect1.right + self->margin.v;
	}

	if (self->margin.h > 0) {
		self->text_rect0.bottom = self->text_rect0.bottom - self->margin.h;
		self->text_rect1.top = self->text_rect1.top + self->margin.h;

	} else if (self->margin.h < 0) {
		self->text_rect0.top = self->text_rect0.top - self->margin.h;
		self->text_rect1.bottom = self->text_rect1.bottom + self->margin.h;
	}
}

int32_t stateful_disp_obj_draw_source(struct stateful_disp_obj *self, struct screen_dc *dc, struct point *pos,
	int32_t index)
{
	return self->source_obj ? DRAWABLE_OPS(self->source_obj)->Draw(self->source_obj, dc, pos, index, 0) : 0;
}
