#include "drwegad.h"

#include <stddef.h>

#include "canvas.h"
#include "../base/log.h"
#include "../base/memory.h"

const struct drw_egad_ops DRW_EGAD_OPS_TABLE = {
	{
		{
			{drw_egad_destroy},
			drw_egad_free,
			drw_egad_release_lock,
			drw_egad_is_unlocked,
			media_base_clone,
			media_base_release_shared,
			media_base_release_source,
		},
		drw_egad_draw_to,
		drw_egad_get_size,
		obj_base_slot9,
		obj_base_slot10,
		drw_egad_get_rect,
	},
};

void DrwEgadInit(struct drw_egad *self, const struct rect *src)
{
	ObjBaseInit(&self->base, NULL);
	self->base.base.base.ops = &DRW_EGAD_OPS_TABLE.base.base.base;

	self->base.base.size = sizeof(struct drw_egad);

	struct rect rect;
	rect.top = 0;
	rect.left = 0;
	rect.right = (src->right - src->left + 7) & ~7;
	rect.bottom = (src->bottom - src->top + 7) & ~7;
	self->canvas = CanvasCreateRect(&rect, 8, 0xFE);

	CanvasLock(self->canvas);

	if (!self->canvas->bits.bits)
		self->base.base.valid = false;

	CanvasUnlock(self->canvas);

	self->canvas->bits.rect = *src;
	self->canvas->bounds = *src;
	self->canvas->bounds = *src;
}

struct drw_egad *DrwEgadCreate(const struct rect *src)
{
	struct drw_egad *self = MemAlloc(1, sizeof(struct drw_egad));
	DrwEgadInit(self, src);

	return self;
}

void DrwEgadDestroy(struct drw_egad **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void drw_egad_destroy(struct object *obj)
{
	struct drw_egad *self = (struct drw_egad *) obj;

	if (self->canvas)
		CanvasDestroy(&self->canvas);

	obj->ops = &OBJ_BASE_OPS_TABLE.base.base;
	obj_base_destroy(obj);
}

int32_t drw_egad_draw_to(struct obj_base *self, struct screen_dc *dc, struct point *pos, int32_t flag)
{
	struct drw_egad *e = (struct drw_egad *) self;

	if (e->canvas)
		CANVAS_OPS(e->canvas)->DrawAt(e->canvas, dc, pos, flag);

	return 0;
}

bool drw_egad_free(struct media_base *self)
{
	struct drw_egad *e = (struct drw_egad *) self;

	if (e->canvas) {
		struct canvas *canvas = e->canvas;

		if (canvas->dib)
			CanvasSetPurgeable(canvas, true);

		if (canvas->dib && canvas->dib_lock_count > 0)
			LogError(LOG_ASSERT, "egad not unlocked in Free()");

		while (canvas->dib && canvas->dib_lock_count > 0) {
			CanvasDibUnlocked(canvas);
			canvas->bits.bits = NULL;
		}
	}

	return true;
}

bool drw_egad_release_lock(struct media_base *self)
{
	struct drw_egad *e = (struct drw_egad *) self;

	if (e->canvas->dib)
		CanvasSetPurgeable(e->canvas, false);

	return e->canvas->dib ? !CanvasDibInvalid(e->canvas) : false;
}

bool drw_egad_is_unlocked(struct media_base *self)
{
	struct drw_egad *e = (struct drw_egad *) self;

	return e->canvas->dib ? CanvasDibInvalid(e->canvas) : true;
}

int32_t drw_egad_get_size(struct obj_base *self, struct point *out)
{
	struct drw_egad *e = (struct drw_egad *) self;

	struct rect bounds = CanvasGetBounds(e->canvas);
	*out = RectSize(&bounds);

	return 0;
}

struct canvas *drw_egad_get_rect(struct obj_base *self, struct rect *out)
{
	struct drw_egad *e = (struct drw_egad *) self;

	*out = CanvasGetBounds(e->canvas);

	return e->canvas;
}
