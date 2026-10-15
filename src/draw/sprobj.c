#include "sprobj.h"

#include <stddef.h>

#include "canvas.h"
#include "../base/memory.h"
#include "sprite.h"
#include "viewsurface.h"

const struct spr_obj_ops SPR_OBJ_OPS_TABLE = {
	{spr_obj_destroy},
	spr_obj_rect_intersects,
	spr_obj_draw_masked,
	spr_obj_render,
	spr_obj_hide,
	spr_obj_show,
	spr_obj_set_image_pos,
	spr_obj_tick,
	spr_obj_move_by,
	spr_obj_set_anchor,
	spr_obj_object_hit_test,
};

static void init_fields(struct spr_obj *self)
{
	self->visible = true;
	self->prev_visible = 0;
	self->dirty = false;
	self->needs_redraw = false;
	self->detach_pending = false;
	self->mask_threshold = 0;
	self->flip_x = false;
	self->region1.p2.h = 0;
	self->region1.p2.v = 0;
	self->tag = 0;
}

void SprObjInit(struct spr_obj *self)
{
	ObjectInit(&self->base);
	self->base.ops = &SPR_OBJ_OPS_TABLE.base;
	self->node.prev = NULL;
	self->node.next = NULL;

	self->owner = NULL;
	self->image = NULL;
	init_fields(self);
}

void SprObjInitMethod(struct spr_obj *self, struct view_surface *owner, struct sprite *image)
{
	self->owner = owner;
	self->image = image;

	if (self->image)
		SPR_OBJ_OPS(self)->SetImagePos(self, 0, 0, 0);

	if (self->owner)
		VIEW_SURFACE_OPS(self->owner)->AddChild(self->owner, self);
}

void SprObjInitOwner(struct spr_obj *self, struct view_surface *owner, struct sprite *image)
{
	ObjectInit(&self->base);
	self->base.ops = &SPR_OBJ_OPS_TABLE.base;
	self->node.prev = NULL;
	self->node.next = NULL;

	self->owner = owner;
	self->image = image;
	init_fields(self);

	if (self->image)
		SPR_OBJ_OPS(self)->SetImagePos(self, 0, 0, 0);

	if (self->owner)
		VIEW_SURFACE_OPS(self->owner)->AddChild(self->owner, self);
}

struct spr_obj *SprObjCreate(struct view_surface *owner, struct sprite *image)
{
	struct spr_obj *self = MemAlloc(1, sizeof(struct spr_obj));
	SprObjInitOwner(self, owner, image);

	return self;
}

void SprObjDestroy(struct spr_obj **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void spr_obj_destroy(struct object *self)
{
	object_destroy(self);
}

struct spr_obj *SprObjFromNode(struct list_node *node)
{
	if (!node)
		return NULL;

	return (struct spr_obj *) ((uint8_t *) node - offsetof(struct spr_obj, node));
}

void SprObjDetach(struct spr_obj *self, bool now)
{
	if (now) {
		VIEW_SURFACE_OPS(self->owner)->RemoveChild(self->owner, self);

	} else {
		self->detach_pending = true;
	}
}

void spr_obj_render(struct spr_obj *self, struct canvas *target)
{
	if (!self->visible)
		return;

	struct point pos = PointMake(self->region1.rect.left, self->region1.rect.top);
	struct drawable *image = &self->image->base;
	DRAWABLE_OPS(image)->DrawAt(image, target, &pos, self->frame, 0, self->flip_x);
}

void spr_obj_draw_masked(struct spr_obj *self, struct canvas *target, struct canvas *src_canvas)
{
	struct rect src_rect = {0};
	struct drawable *image = &self->image->base;
	struct canvas *frame_canvas = DRAWABLE_OPS(image)->GetRectAt(image, self->frame, &src_rect);

	struct rect dst_rect = src_rect;
	dst_rect.right = dst_rect.right - dst_rect.left;
	dst_rect.bottom = dst_rect.bottom - dst_rect.top;
	dst_rect.top = 0;
	dst_rect.left = 0;
	RectOffset(&dst_rect, self->region1.rect.left, self->region1.rect.top);

	struct rect clip_rect = CanvasGetBounds(target);

	struct point pos = RectTopLeft(&dst_rect);
	CANVAS_OPS(target)->BlitMasked(target, frame_canvas, src_canvas, &src_rect, &pos, &clip_rect,
		self->mask_threshold, self->flip_x);
}

void spr_obj_hide(struct spr_obj *self)
{
	self->visible = false;
	self->dirty = true;
}

void spr_obj_show(struct spr_obj *self)
{
	self->visible = true;
	self->dirty = true;
}

void spr_obj_move_by(struct spr_obj *self, int32_t dh, int32_t dv)
{
	self->region1.rect.left += dh;
	self->region1.rect.right += dh;
	self->region1.rect.top += dv;
	self->region1.rect.bottom += dv;
	self->region1.p2.v += dv;
	self->region1.p2.h += dh;

	if (dh || dv)
		self->dirty = true;
}

void spr_obj_set_anchor(struct spr_obj *self, int32_t h_g, int32_t v_h)
{
	int32_t image_off_h = self->region1.p1.h;
	int32_t image_off_v = self->region1.p1.v;
	self->region1.p2.h = h_g;
	self->region1.p2.v = v_h;

	int32_t width = self->region1.rect.right - self->region1.rect.left;
	int32_t height = self->region1.rect.bottom - self->region1.rect.top;
	self->region1.rect = RectMake(0, 0, height, width);

	int32_t shift_v = self->region1.p2.v - image_off_v;
	int32_t shift_h = self->region1.p2.h - image_off_h;
	self->region1.rect.left += shift_h;
	self->region1.rect.right += shift_h;
	self->region1.rect.top += shift_v;
	self->region1.rect.bottom += shift_v;
	self->dirty = true;
}

void spr_obj_set_image_pos(struct spr_obj *self, int32_t frame, int32_t h, int32_t v)
{
	struct point frame_size = {0};
	self->frame = frame;
	self->region1.p1.h = h;
	self->region1.p1.v = v;

	struct drawable *image = &self->image->base;
	DRAWABLE_OPS(image)->GetSizeAt(image, &frame_size, frame);

	self->region1.rect = RectMake(0, 0, frame_size.v, frame_size.h);

	int32_t shift_v = self->region1.p2.v - v;
	int32_t shift_h = self->region1.p2.h - h;
	self->region1.rect.left += shift_h;
	self->region1.rect.right += shift_h;
	self->region1.rect.top += shift_v;
	self->region1.rect.bottom += shift_v;
	self->dirty = true;
}

void spr_obj_tick(struct spr_obj *self)
{
	(void) self;
}

bool spr_obj_object_hit_test(struct spr_obj *self, struct point *pt)
{
	struct rect frame_rect = {0};
	struct point frame_size = {0};
	bool is_hit = false;

	struct drawable *image = &self->image->base;
	DRAWABLE_OPS(image)->GetSizeAt(image, &frame_size, self->frame);

	frame_rect.right = frame_rect.left + frame_size.h;
	frame_rect.bottom = frame_rect.top + frame_size.v;

	struct canvas *frame_canvas = DRAWABLE_OPS(image)->GetRectAt(image, self->frame, &frame_rect);

	if (!frame_canvas)
		return is_hit;

	CanvasLock(frame_canvas);

	const uint8_t *pixels = frame_canvas->bits.bits;

	struct point local_pos;
	local_pos.h = pt->h - self->region1.rect.left;
	local_pos.v = pt->v - self->region1.rect.top;
	int32_t pixel_index = frame_canvas->bits.stride * local_pos.v + local_pos.h;

	if (frame_rect.right * frame_rect.bottom > pixel_index) {
		if (pixels[pixel_index] != 0)
			is_hit = true;
	}

	CanvasUnlock(frame_canvas);

	return is_hit;
}

bool spr_obj_rect_intersects(struct spr_obj *self, struct spr_obj *other)
{
	struct rect tmp;

	return RectIntersect(&self->region1.rect, &other->region1.rect, &tmp);
}
