#pragma once

struct object;

struct object_ops {
	void (*destroy)(struct object *self);
};

struct object {
	const struct object_ops *ops;
};

#define OBJECT_OPS(p) ((const struct object_ops *) ((const struct object *) (p))->ops)

void ObjectInit(struct object *self);
void ObjectDelete(void *obj);
void ObjectDestroy(void *obj_mod);
void object_destroy(struct object *self);
