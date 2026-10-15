#pragma once

#include <stdint.h>

#include "../draw/sprobj.h"

struct sprite;
struct view_surface;

struct animal_ops {
	struct spr_obj_ops base;
};

struct animal {
	struct spr_obj base;
	int32_t delay;
};

#define ANIMAL_OPS(p) ((const struct animal_ops *) ((const struct object *) (p))->ops)

extern const struct animal_ops ANIMAL_OPS_TABLE;

void AnimalInit(struct animal *self, struct view_surface *owner, struct sprite *image);
struct animal *AnimalCreate(struct view_surface *owner, struct sprite *image);
void AnimalDestroy(struct animal **ctx_mod);
void animal_destroy(struct object *self);

void animal_tick(struct spr_obj *self);
