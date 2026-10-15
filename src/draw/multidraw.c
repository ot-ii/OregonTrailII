#include "multidraw.h"

#include <stddef.h>

#include "../base/memory.h"

const struct multi_draw_ops MULTI_DRAW_OPS_TABLE = {
	{
		{
			{
				{multi_draw_destroy},
				multi_draw_free,
				multi_draw_release_lock,
				multi_draw_is_unlocked,
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
		multi_draw_draw,
		multi_draw_get_size_at,
		multi_draw_get_rect_at,
	},
};

void MultiDrawInit(struct multi_draw *self, struct server *arg)
{
	DrawableInit(&self->base, arg);
	self->base.base.base.base.ops = &MULTI_DRAW_OPS_TABLE.base.base.base.base;

	self->slots = NULL;
	self->base.base.base.size = sizeof(struct multi_draw);
}

struct multi_draw *MultiDrawCreate(struct server *arg)
{
	struct multi_draw *self = MemAlloc(1, sizeof(struct multi_draw));
	MultiDrawInit(self, arg);

	return self;
}

void MultiDrawDestroy(struct multi_draw **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void multi_draw_destroy(struct object *obj)
{
	struct multi_draw *self = (struct multi_draw *) obj;

	if (self->slots) {
		for (int32_t i = 0; self->base.item_count > i; ++i) {
			if (self->slots[i])
				ReleaseDrawable((struct media_base *) self->slots[i], false);
		}

		MemFree(self->slots);
	}

	obj->ops = &DRAWABLE_OPS_TABLE.base.base.base;
	drawable_destroy(obj);
}

int32_t multi_draw_draw(struct drawable *self, struct screen_dc *dc, struct point *pos, int32_t index,
	int32_t flag)
{
	struct multi_draw *m = (struct multi_draw *) self;
	struct drawable *slot;

	if (self->item_count > index && (slot = m->slots[index]) != NULL)
		return OBJ_BASE_OPS(slot)->DrawTo((struct obj_base *) slot, dc, pos, flag);

	return 0;
}

bool multi_draw_free(struct media_base *self)
{
	struct multi_draw *m = (struct multi_draw *) self;
	bool all = true;

	if (m->slots) {
		for (int32_t i = 0; m->base.item_count > i; ++i) {
			if (m->slots[i]) {
				struct media_base *slot = (struct media_base *) m->slots[i];

				if (!MEDIA_BASE_OPS(slot)->Free(slot))
					all = false;
			}
		}
	}

	return all;
}

bool multi_draw_release_lock(struct media_base *self)
{
	struct multi_draw *m = (struct multi_draw *) self;

	if (m->slots) {
		for (int32_t i = 0; m->base.item_count > i; ++i) {
			if (m->slots[i]) {
				struct media_base *slot = (struct media_base *) m->slots[i];

				if (!MEDIA_BASE_OPS(slot)->ReleaseLock(slot))
					return false;
			}
		}
	}

	return true;
}

bool multi_draw_is_unlocked(struct media_base *self)
{
	struct multi_draw *m = (struct multi_draw *) self;

	if (m->slots) {
		for (int32_t i = 0; m->base.item_count > i; ++i) {
			if (m->slots[i]) {
				struct media_base *slot = (struct media_base *) m->slots[i];

				if (!MEDIA_BASE_OPS(slot)->IsUnlocked(slot))
					return false;
			}
		}
	}

	return true;
}

int32_t multi_draw_get_size_at(struct drawable *self, struct point *out, int32_t b)
{
	struct multi_draw *m = (struct multi_draw *) self;
	struct obj_base *slot = (struct obj_base *) m->slots[b];

	return OBJ_BASE_OPS(slot)->GetSize(slot, out);
}

struct canvas *multi_draw_get_rect_at(struct drawable *self, int32_t a, struct rect *b)
{
	struct multi_draw *m = (struct multi_draw *) self;
	struct obj_base *slot = (struct obj_base *) m->slots[a];

	return OBJ_BASE_OPS(slot)->GetRect(slot, b);
}

void MultiDrawAllocSlots(struct multi_draw *self)
{
	self->slots = MemAlloc((size_t) self->base.item_count, sizeof(struct drawable *));

	for (int32_t i = 0; self->base.item_count > i; ++i)
		self->slots[i] = NULL;
}
