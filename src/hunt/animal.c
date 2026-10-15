#include "animal.h"

#include "../base/memory.h"

const struct animal_ops ANIMAL_OPS_TABLE = {
	{
		{animal_destroy},
		spr_obj_rect_intersects,
		spr_obj_draw_masked,
		spr_obj_render,
		spr_obj_hide,
		spr_obj_show,
		spr_obj_set_image_pos,
		animal_tick,
		spr_obj_move_by,
		spr_obj_set_anchor,
		spr_obj_object_hit_test,
	},
};

void AnimalInit(struct animal *self, struct view_surface *owner, struct sprite *image)
{
	SprObjInitOwner(&self->base, owner, image);
	self->base.base.ops = &ANIMAL_OPS_TABLE.base.base;

	self->base.mask_threshold = 1;
	self->delay = 0;
}

struct animal *AnimalCreate(struct view_surface *owner, struct sprite *image)
{
	struct animal *self = MemAlloc(1, sizeof(struct animal));
	AnimalInit(self, owner, image);

	return self;
}

void AnimalDestroy(struct animal **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void animal_destroy(struct object *self)
{
	self->ops = &SPR_OBJ_OPS_TABLE.base;
	spr_obj_destroy(self);
}

void animal_tick(struct spr_obj *self)
{
	struct animal *a = (struct animal *) self;

	if (a->delay > 0) {
		a->delay--;

	} else if (self->visible) {
		SPR_OBJ_OPS(self)->Hide(self);
	}
}
