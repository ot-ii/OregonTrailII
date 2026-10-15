#include "../dc.h"

#include <windows.h>

#include "../screendc.h"

const struct dc_ops DC_OPS_TABLE = {
	{dc_destroy},
};

void DcInit(struct dc *self, struct screen_dc *src)
{
	ObjectInit(&self->base);
	self->base.ops = &DC_OPS_TABLE.base;

	self->hdc = src->hdc;
}

void dc_destroy(struct object *obj)
{
	struct dc *self = (struct dc *) obj;

	SelectClipRgn((HDC) self->hdc, NULL);

	object_destroy(obj);
}
