#include "multiimage.h"

#include <stddef.h>

#include "canvas.h"
#include "drwegad.h"
#include "../data/image.h"
#include "../base/memory.h"

const struct multi_image_ops MULTI_IMAGE_OPS_TABLE = {
	{
		{
			{
				{
					{multi_image_destroy},
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
	},
};

static struct cbmp *image_create(struct rsrc *body)
{
	return body ? CbmpCreate(body) : NULL;
}

void MultiImageInit(struct multi_image *self, struct rsrc *body, struct server *arg)
{
	MultiDrawInit(&self->base, arg);
	self->base.base.base.base.base.ops = &MULTI_IMAGE_OPS_TABLE.base.base.base.base.base;

	struct drawable *d = &self->base.base;
	struct media_base *mb = &d->base.base;

	struct rect frame_rect = {0};

	mb->size = sizeof(struct multi_image);
	struct cbmp *image = image_create(body);

	if (image) {
		d->item_count = CbmpFrameCount(image);
		MultiDrawAllocSlots(&self->base);

		for (int32_t index = 0; d->item_count > index; ++index) {
			int32_t w = 0;
			int32_t h = 0;
			CbmpFrameSize(image, index, &w, &h);

			frame_rect.right = frame_rect.left + w;
			frame_rect.bottom = frame_rect.top + h;

			struct drw_egad *frame = DrwEgadCreate(&frame_rect);
			self->base.slots[index] = (struct drawable *) frame;

			if (!frame->base.base.valid) {
				mb->valid = false;
				break;
			}

			struct obj_base *fo = &frame->base;
			CanvasDrawFrame(OBJ_BASE_OPS(fo)->GetRect(fo, &frame_rect), image, index);
		}

	} else {
		mb->valid = false;
	}

	CbmpDestroy(&image);
}

struct multi_image *MultiImageCreate(struct rsrc *body, struct server *arg)
{
	struct multi_image *self = MemAlloc(1, sizeof(struct multi_image));
	MultiImageInit(self, body, arg);

	return self;
}

void MultiImageDestroy(struct multi_image **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void multi_image_destroy(struct object *self)
{
	self->ops = &MULTI_DRAW_OPS_TABLE.base.base.base.base;
	multi_draw_destroy(self);
}

void MultiImageLoadFrames(struct multi_image *self, struct rsrc *body)
{
	struct drawable *d = &self->base.base;

	struct rect frame_rect = {0};

	d->base.base.size = sizeof(struct multi_image);
	struct cbmp *image = image_create(body);

	if (image) {
		d->item_count = CbmpFrameCount(image);

		for (int32_t index = 0; d->item_count > index; ++index) {
			int32_t w = 0;
			int32_t h = 0;
			CbmpFrameSize(image, index, &w, &h);

			frame_rect.right = frame_rect.left + w;
			frame_rect.bottom = frame_rect.top + h;

			struct obj_base *fo = (struct obj_base *) self->base.slots[index];
			CanvasDrawFrameMasked(OBJ_BASE_OPS(fo)->GetRect(fo, &frame_rect), image, index);
		}
	}

	CbmpDestroy(&image);
}
