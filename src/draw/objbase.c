#include "objbase.h"

#include <stddef.h>

const struct obj_base_ops OBJ_BASE_OPS_TABLE = {
	{
		{obj_base_destroy},
		media_base_free,
		media_base_release_lock,
		media_base_is_unlocked,
		media_base_clone,
		media_base_release_shared,
		media_base_release_source,
	},
	obj_base_draw_to,
	obj_base_get_size,
	obj_base_slot9,
	obj_base_slot10,
	obj_base_get_rect,
};

void ObjBaseInit(struct obj_base *self, struct server *server)
{
	MediaBaseInit(&self->base, server);
	self->base.base.ops = &OBJ_BASE_OPS_TABLE.base.base;

	self->base.size = sizeof(struct obj_base);
}

void obj_base_destroy(struct object *self)
{
	self->ops = &MEDIA_BASE_OPS_TABLE.base;
	media_base_destroy(self);
}

int32_t obj_base_get_size(struct obj_base *self, struct point *out)
{
	(void) self;

	out->h = 0;
	out->v = 0;

	return 0;
}

int32_t obj_base_draw_to(struct obj_base *self, struct screen_dc *dc, struct point *pos, int32_t flag)
{
	(void) self;
	(void) dc;
	(void) pos;
	(void) flag;

	return 0;
}

int32_t obj_base_slot9(struct obj_base *self)
{
	(void) self;

	return 0;
}

int32_t obj_base_slot10(struct obj_base *self, int32_t arg)
{
	(void) self;
	(void) arg;

	return 0;
}

struct canvas *obj_base_get_rect(struct obj_base *self, struct rect *out)
{
	(void) self;
	(void) out;

	return NULL;
}
