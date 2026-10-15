#include "shapebase.h"

#include <stddef.h>

const struct shape_base_ops SHAPE_BASE_OPS_TABLE = {
	{shape_base_destroy},
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

void ShapeBaseInit(struct shape_base *self)
{
	ObjectInit(&self->base);
	self->base.ops = &SHAPE_BASE_OPS_TABLE.base;
}

void shape_base_destroy(struct object *self)
{
	object_destroy(self);
}
