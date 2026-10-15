#include "object.h"

#include <stddef.h>

#include "memory.h"

static const struct object_ops OBJECT_OPS_TABLE = {
	object_destroy,
};

void ObjectInit(struct object *self)
{
	self->ops = &OBJECT_OPS_TABLE;
}

void object_destroy(struct object *self)
{
	(void) self;
}

void ObjectDelete(void *obj)
{
	struct object *self = obj;

	if (!self)
		return;

	self->ops->destroy(self);
	MemFree(self);
}

void ObjectDestroy(void *obj_mod)
{
	struct object **mod = obj_mod;

	if (!mod || !*mod)
		return;

	struct object *self = *mod;
	*mod = NULL;

	ObjectDelete(self);
}
