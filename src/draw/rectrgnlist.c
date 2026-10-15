#include "rectrgnlist.h"

#include "../base/memory.h"

const struct rect_rgn_list_ops RECT_RGN_LIST_OPS_TABLE = {
	{
		{rect_rgn_list_destroy},
		rect_rgn_list_reset,
		rect_rgn_list_count,
		rect_rgn_list_get_rect_at,
		rect_rgn_list_add_rect,
		rect_rgn_list_intersects_rect,
	},
};

void RectRgnListInit(struct rect_rgn_list *self)
{
	ShapeBaseInit(&self->base);
	self->base.base.ops = &RECT_RGN_LIST_OPS_TABLE.base.base;

	SHAPE_BASE_OPS(self)->Reset(&self->base);
	self->flag = false;
}

struct rect_rgn_list *RectRgnListCreate(void)
{
	struct rect_rgn_list *self = MemAlloc(1, sizeof(struct rect_rgn_list));
	RectRgnListInit(self);

	return self;
}

void RectRgnListDestroy(struct rect_rgn_list **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void rect_rgn_list_destroy(struct object *self)
{
	self->ops = &SHAPE_BASE_OPS_TABLE.base;
	shape_base_destroy(self);
}

void rect_rgn_list_reset(struct shape_base *base)
{
	struct rect_rgn_list *self = (struct rect_rgn_list *) base;

	self->count = 0;
	self->head.next = &self->head;
}

bool rect_rgn_list_intersects_rect(struct shape_base *base, struct rect *rect)
{
	struct rect_rgn_list *self = (struct rect_rgn_list *) base;

	int32_t i = self->count;

	while (i > 0) {
		--i;
		struct rect current = self->entries[i].rect;
		struct rect inter;

		if (RectIntersect(rect, &current, &inter))
			return true;
	}

	return false;
}

void rect_rgn_list_add_rect(struct shape_base *base, struct rect *rect)
{
	struct rect_rgn_list *self = (struct rect_rgn_list *) base;

	struct rect snapped = *rect;
	snapped.left &= ~3;
	int32_t right_rem = snapped.right & 3;
	snapped.right = (snapped.right & ~3) + (right_rem == 0 ? 0 : 4);

	struct rect_entry *list_head = &self->head;
	struct rect_entry *node = &self->entries[self->count];

	for (int32_t index = self->count; index > 0; --index) {
		node--;
		self->bounds = node->rect;

		struct rect r2;

		if (RectIntersect(&snapped, &self->bounds, &r2)) {
			if (self->bounds.right < snapped.right) {
				if ((snapped.right - self->bounds.left) >> 1 < self->bounds.right - snapped.left)
					goto merge;

			} else {
				if ((self->bounds.right - snapped.left) >> 1 < snapped.right - self->bounds.left)
					goto merge;
			}

			if (self->bounds.bottom < snapped.bottom) {
				if ((snapped.bottom - self->bounds.top) >> 1 < self->bounds.bottom - snapped.top)
					goto merge;

			} else {
				if ((self->bounds.bottom - snapped.top) >> 1 < snapped.bottom - self->bounds.top)
					goto merge;
			}
		}
	}

	if (self->count >= RECT_RGN_LIST_MAX)
		return;

	node = &self->entries[self->count];
	++self->count;
	node->rect = snapped;
	node->next = list_head->next;
	list_head->next = node;
	return;

merge:
	node->rect = RectUnion(&snapped, &self->bounds);
}

int32_t rect_rgn_list_count(struct shape_base *base)
{
	struct rect_rgn_list *self = (struct rect_rgn_list *) base;

	return self->count;
}

void rect_rgn_list_get_rect_at(struct shape_base *base, struct rect *out, int32_t index)
{
	struct rect_rgn_list *self = (struct rect_rgn_list *) base;

	*out = self->entries[index].rect;
}
