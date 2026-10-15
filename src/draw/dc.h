#pragma once

#include "../base/object.h"

struct screen_dc;

struct dc_ops {
	struct object_ops base;
};

struct dc {
	struct object base;
	void *hdc;
};

#define DC_OPS(p) ((const struct dc_ops *) ((const struct object *) (p))->ops)

extern const struct dc_ops DC_OPS_TABLE;

void DcInit(struct dc *self, struct screen_dc *src);
void dc_destroy(struct object *self);
