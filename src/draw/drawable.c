#include "drawable.h"

#include <stddef.h>

#include "canvas.h"

const struct drawable_ops DRAWABLE_OPS_TABLE = {
	{
		{
			{drawable_destroy},
			media_base_free,
			media_base_release_lock,
			media_base_is_unlocked,
			media_base_clone,
			media_base_release_shared,
			media_base_release_source,
		},
		drawable_draw_to,
		drawable_get_size,
		obj_base_slot9,
		obj_base_slot10,
		drawable_get_rect,
	},
	drawable_draw_at,
	drawable_draw,
	drawable_get_size_at,
	drawable_get_rect_at,
};

void DrawableInit(struct drawable *self, struct server *server)
{
	ObjBaseInit(&self->base, server);
	self->base.base.base.ops = &DRAWABLE_OPS_TABLE.base.base.base;

	self->base.base.size = sizeof(struct drawable);
	self->item_count = 0;
}

void drawable_destroy(struct object *self)
{
	self->ops = &OBJ_BASE_OPS_TABLE.base.base;
	obj_base_destroy(self);
}

int32_t drawable_draw_to(struct obj_base *self, struct screen_dc *dc, struct point *pos, int32_t flag)
{
	struct drawable *d = (struct drawable *) self;

	return DRAWABLE_OPS(d)->Draw(d, dc, pos, 0, flag);
}

int32_t drawable_draw(struct drawable *self, struct screen_dc *dc, struct point *pos, int32_t index, int32_t flag)
{
	(void) self;
	(void) dc;
	(void) pos;
	(void) index;
	(void) flag;

	return 0;
}

int32_t drawable_draw_at(struct drawable *self, struct canvas *dc, struct point *pos, int32_t index, int32_t y,
	int32_t flag)
{
	(void) y;

	struct rect src = {0};
	struct canvas *source = DRAWABLE_OPS(self)->GetRectAt(self, index, &src);

	struct rect cdest = src;
	cdest.right -= cdest.left;
	cdest.bottom -= cdest.top;
	cdest.top = 0;
	cdest.left = 0;
	cdest.left += pos->h;
	cdest.right += pos->h;
	cdest.top += pos->v;
	cdest.bottom += pos->v;

	struct rect box = CanvasGetBounds(dc);

	struct point at = RectTopLeft(&cdest);
	CANVAS_OPS(dc)->BlitImage(dc, source, &src, &at, &box, flag != 0);

	return 0;
}

int32_t drawable_get_size_at(struct drawable *self, struct point *out, int32_t index)
{
	(void) self;
	(void) index;

	out->h = 0;
	out->v = 0;

	return 0;
}

int32_t drawable_get_size(struct obj_base *self, struct point *out)
{
	struct drawable *d = (struct drawable *) self;

	return DRAWABLE_OPS(d)->GetSizeAt(d, out, 0);
}

struct canvas *drawable_get_rect(struct obj_base *self, struct rect *out)
{
	struct drawable *d = (struct drawable *) self;

	return DRAWABLE_OPS(d)->GetRectAt(d, 0, out);
}

struct canvas *drawable_get_rect_at(struct drawable *self, int32_t index, struct rect *out)
{
	(void) self;
	(void) index;
	(void) out;

	return NULL;
}
