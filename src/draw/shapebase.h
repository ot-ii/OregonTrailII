#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/object.h"

struct rect;
struct shape_base;

struct shape_base_ops {
	struct object_ops base;
	void (*Reset)(struct shape_base *self);
	int32_t (*Count)(struct shape_base *self);
	void (*GetRectAt)(struct shape_base *self, struct rect *out, int32_t index);
	void (*AddRect)(struct shape_base *self, struct rect *rect);
	bool (*IntersectsRect)(struct shape_base *self, struct rect *rect);
};

struct shape_base {
	struct object base;
};

#define SHAPE_BASE_OPS(p) ((const struct shape_base_ops *) ((const struct object *) (p))->ops)

void ShapeBaseInit(struct shape_base *self);
void shape_base_destroy(struct object *self);

extern const struct shape_base_ops SHAPE_BASE_OPS_TABLE;