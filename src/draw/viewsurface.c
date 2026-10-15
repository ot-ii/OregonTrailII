#include "viewsurface.h"

#include <stddef.h>

#include "canvas.h"
#include "drawable.h"
#include "../base/list.h"
#include "../base/memory.h"
#include "rectrgnlist.h"
#include "sprobj.h"
#include "windc.h"

const struct view_surface_ops VIEW_SURFACE_OPS_TABLE = {
	{view_surface_destroy},
	view_surface_render_pipeline,
	view_surface_set_background_source,
	view_surface_set_cover_mask,
	view_surface_head,
	view_surface_find_by_id,
	view_surface_hit_test,
	view_surface_add_child,
	view_surface_remove_child,
	view_surface_mark_all_dirty,
	view_surface_remove_all_children,
	view_surface_purge_dead,
	view_surface_draw_region,
	view_surface_mark_hit,
	view_surface_flush_to_window,
	view_surface_redraw_dirty,
	view_surface_blit,
	view_surface_for_each_child,
};

static struct shape_base *rgn(struct rect_rgn_list *list)
{
	return &list->base;
}

static struct spr_obj *first_child(struct view_surface *self)
{
	return SprObjFromNode(self->children->head);
}

static struct spr_obj *next_child(struct spr_obj *obj)
{
	return SprObjFromNode(obj->node.next);
}

void ViewSurfaceInitDefault(struct view_surface *self)
{
	ObjectInit(&self->base);
	self->base.ops = &VIEW_SURFACE_OPS_TABLE.base;
}

void ViewSurfaceInit(struct view_surface *self, struct rect *rect, struct palette *palette, bool has_overlay,
	bool has_mask)
{
	(void) palette;

	ObjectInit(&self->base);
	self->base.ops = &VIEW_SURFACE_OPS_TABLE.base;

	self->bounds = *rect;
	self->origin.v = rect->top;
	self->origin.h = rect->left;
	rect->right -= rect->left;
	rect->bottom -= rect->top;
	rect->top = 0;
	rect->left = 0;
	self->rect_a = *rect;
	self->rect_b = *rect;
	self->has_background = has_mask;

	if (self->has_background) {
		self->background = CanvasCreateRect(rect, 8, 0);
		self->bounds_list = RectRgnListCreate();
	}

	self->main_canvas = CanvasCreateRect(rect, 8, 0);

	if (has_overlay) {
		self->cover_mask = CanvasCreateRect(rect, 8, 0);

	} else {
		self->cover_mask = NULL;
	}

	self->blit_source = NULL;
	self->cover_source = NULL;
	self->dirty_list = RectRgnListCreate();
	self->clip_list = RectRgnListCreate();
	self->children = MemAlloc(1, sizeof(struct list));
	ListInit(self->children);
}

struct view_surface *ViewSurfaceCreate(struct rect *rect, struct palette *palette, bool has_overlay,
	bool has_mask)
{
	struct view_surface *self = MemAlloc(1, sizeof(struct view_surface));
	ViewSurfaceInit(self, rect, palette, has_overlay, has_mask);

	return self;
}

void ViewSurfaceDestroy(struct view_surface **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void view_surface_destroy(struct object *obj)
{
	struct view_surface *self = (struct view_surface *) obj;

	if (self->has_background) {
		CanvasDestroy(&self->background);
		RectRgnListDestroy(&self->bounds_list);
	}

	CanvasDestroy(&self->main_canvas);

	if (self->cover_mask)
		CanvasDestroy(&self->cover_mask);

	VIEW_SURFACE_OPS(self)->RemoveAllChildren(self);
	MemFree(self->children);
	self->children = NULL;
	RectRgnListDestroy(&self->dirty_list);
	RectRgnListDestroy(&self->clip_list);

	object_destroy(obj);
}

static bool in_rect(struct spr_obj *sprite, struct point *pt)
{
	if (sprite->region1.rect.left > pt->h)
		return false;

	if (sprite->region1.rect.right < pt->h)
		return false;

	if (sprite->region1.rect.top > pt->v)
		return false;

	if (sprite->region1.rect.bottom < pt->v)
		return false;

	return true;
}

struct spr_obj *view_surface_hit_test(struct view_surface *self, struct point *pt)
{
	for (struct spr_obj *sprite = SprObjFromNode(self->children->tail); sprite;) {
		if (in_rect(sprite, pt) && SPR_OBJ_OPS(sprite)->ObjectHitTest(sprite, pt))
			return sprite;

		sprite = SprObjFromNode(sprite->node.prev);
	}

	return NULL;
}

struct spr_obj *ViewSurfaceHitTestTyped(struct view_surface *self, struct point *pt, int32_t type)
{
	for (struct spr_obj *sprite = SprObjFromNode(self->children->tail); sprite;) {
		if (sprite->tag == type) {
			if (in_rect(sprite, pt) && SPR_OBJ_OPS(sprite)->ObjectHitTest(sprite, pt))
				return sprite;
		}

		sprite = SprObjFromNode(sprite->node.prev);
	}

	return NULL;
}

void view_surface_render_pipeline(struct view_surface *self, struct win_dc *window, bool flush)
{
	const struct view_surface_ops *ops = VIEW_SURFACE_OPS(self);

	if (flush)
		ops->ForEachChild(self);

	SHAPE_BASE_OPS(self->clip_list)->Reset(rgn(self->clip_list));
	ops->DrawRegion(self, self->clip_list);
	ops->MarkHit(self, self->clip_list);
	SHAPE_BASE_OPS(self->dirty_list)->Reset(rgn(self->dirty_list));
	ops->RedrawDirty(self, self->dirty_list);

	if (window && flush)
		ops->FlushToWindow(self, window, self->dirty_list);

	ops->PurgeDead(self);
}

void view_surface_purge_dead(struct view_surface *self)
{
	for (struct spr_obj *cursor = first_child(self); cursor;) {
		struct spr_obj *sprite = cursor;
		cursor = next_child(cursor);

		if (sprite->detach_pending)
			VIEW_SURFACE_OPS(self)->RemoveChild(self, sprite);
	}
}

void view_surface_for_each_child(struct view_surface *self)
{
	for (struct spr_obj *sprite = first_child(self); sprite;) {
		SPR_OBJ_OPS(sprite)->Tick(sprite);
		sprite = next_child(sprite);
	}
}

void view_surface_flush_to_window(struct view_surface *self, struct win_dc *windc, struct rect_rgn_list *region)
{
	struct rect src_rect = {0};

	CanvasLock(self->main_canvas);

	if (self->has_background) {
		int32_t count = SHAPE_BASE_OPS(self->bounds_list)->Count(rgn(self->bounds_list));

		if (count) {
			for (int32_t i = 0; count > i; ++i) {
				SHAPE_BASE_OPS(self->bounds_list)->GetRectAt(rgn(self->bounds_list), &src_rect, i);
				SHAPE_BASE_OPS(region)->AddRect(rgn(region), &src_rect);
			}
		}

		SHAPE_BASE_OPS(self->bounds_list)->Reset(rgn(self->bounds_list));
	}

	int32_t count = SHAPE_BASE_OPS(region)->Count(rgn(region));

	for (int32_t i = 0; count > i; ++i) {
		SHAPE_BASE_OPS(region)->GetRectAt(rgn(region), &src_rect, i);

		struct rect dst_rect = src_rect;
		dst_rect.left = dst_rect.left + self->origin.h;
		dst_rect.right = dst_rect.right + self->origin.h;
		dst_rect.top = dst_rect.top + self->origin.v;
		dst_rect.bottom = dst_rect.bottom + self->origin.v;

		WinDcBlitCanvasGuarded(windc, self->main_canvas, &src_rect, &dst_rect, 0, &self->bounds);
	}

	CanvasUnlock(self->main_canvas);
}

void ViewSurfaceInvalidateBoth(struct view_surface *self, struct screen_dc *dc, struct rect pt, struct rect src)
{
	if (self->has_background) {
		ViewSurfaceInvalidateRect(self, src);

		struct point pos = RectTopLeft(&src);
		CANVAS_OPS(self->background)->BlitImage(self->background, (struct canvas *) dc, &pt, &pos, &self->rect_b,
			false);
		CANVAS_OPS(self->main_canvas)->BlitImage(self->main_canvas, (struct canvas *) dc, &pt, &pos, &self->rect_b,
			false);
	}
}

void ViewSurfaceInvalidateRect(struct view_surface *self, struct rect rect)
{
	if (self->has_background) {
		struct rect bounds = CanvasGetBounds(self->main_canvas);
		RectIntersect(&rect, &bounds, &rect);
		SHAPE_BASE_OPS(self->bounds_list)->AddRect(rgn(self->bounds_list), &rect);
	}
}

void view_surface_set_background_source(struct view_surface *self, struct drawable *source, int32_t index)
{
	struct rect src_rect = {0};

	self->blit_source = source;
	struct canvas *image = DRAWABLE_OPS(source)->GetRectAt(source, index, &src_rect);

	struct rect dst_rect = src_rect;
	dst_rect.right -= dst_rect.left;
	dst_rect.bottom -= dst_rect.top;
	dst_rect.top = 0;
	dst_rect.left = 0;

	if (self->has_background)
		CANVAS_OPS(self->background)->StretchBlit(self->background, image, &src_rect, &dst_rect, 0, &self->rect_b);

	CANVAS_OPS(self->main_canvas)->StretchBlit(self->main_canvas, image, &src_rect, &dst_rect, 0, &self->rect_b);
}

void view_surface_set_cover_mask(struct view_surface *self, struct drawable *source, int32_t index)
{
	struct rect src_rect = {0};

	self->cover_source = source;

	if (self->cover_mask) {
		struct canvas *image = DRAWABLE_OPS(source)->GetRectAt(source, index, &src_rect);

		struct rect dst_rect = src_rect;
		dst_rect.right -= dst_rect.left;
		dst_rect.bottom -= dst_rect.top;
		dst_rect.top = 0;
		dst_rect.left = 0;

		CANVAS_OPS(self->cover_mask)->StretchBlit(self->cover_mask, image, &src_rect, &dst_rect, 0, NULL);
	}
}

void view_surface_add_child(struct view_surface *self, struct spr_obj *node)
{
	ListInsertTail(self->children, &node->node);
}

void view_surface_remove_child(struct view_surface *self, struct spr_obj *node)
{
	ListRemove(self->children, &node->node);

	if (node)
		ObjectDelete(node);
}

struct spr_obj *view_surface_head(struct view_surface *self)
{
	return first_child(self);
}

struct spr_obj *view_surface_find_by_id(struct view_surface *self, int32_t id)
{
	struct spr_obj *sprite = first_child(self);

	while (sprite) {
		if (sprite->tag == id)
			return sprite;

		sprite = next_child(sprite);
	}

	return NULL;
}

void view_surface_mark_all_dirty(struct view_surface *self)
{
	for (struct spr_obj *sprite = first_child(self); sprite;) {
		sprite->dirty = true;
		sprite = next_child(sprite);
	}
}

void view_surface_blit(struct view_surface *self, struct rect *rect)
{
	struct rect src_rect = *rect;
	struct rect source_bounds = {0};

	if (self->has_background) {
		CANVAS_OPS(self->main_canvas)->StretchBlit(self->main_canvas, self->background, rect, rect, 0, NULL);

	} else {
		struct canvas *image = DRAWABLE_OPS(self->blit_source)->GetRectAt(self->blit_source, 0, &source_bounds);
		int32_t offset_v = source_bounds.top;
		int32_t offset_h = source_bounds.left;
		src_rect.left += offset_h;
		src_rect.right += offset_h;
		src_rect.top += offset_v;
		src_rect.bottom += offset_v;
		CANVAS_OPS(self->main_canvas)->StretchBlit(self->main_canvas, image, &src_rect, rect, 0, NULL);
	}
}

void view_surface_draw_region(struct view_surface *self, struct rect_rgn_list *region)
{
	struct rect rect = {0};
	struct rect bounds = CanvasGetBounds(self->main_canvas);

	for (struct spr_obj *sprite = first_child(self); sprite;) {
		if (sprite->dirty) {
			if (sprite->prev_visible) {
				RectIntersect(&sprite->region2.rect, &bounds, &rect);
				SHAPE_BASE_OPS(region)->AddRect(rgn(region), &rect);
			}
		}

		sprite = next_child(sprite);
	}

	int32_t count = SHAPE_BASE_OPS(region)->Count(rgn(region));

	for (int32_t i = 0; i < count; ++i) {
		SHAPE_BASE_OPS(region)->GetRectAt(rgn(region), &rect, i);
		VIEW_SURFACE_OPS(self)->Blit(self, &rect);
	}
}

void view_surface_redraw_dirty(struct view_surface *self, struct rect_rgn_list *region)
{
	struct rect rect = {0};
	struct rect bounds = CanvasGetBounds(self->main_canvas);

	for (struct spr_obj *sprite = first_child(self); sprite;) {
		if (!sprite->detach_pending && sprite->visible && (sprite->dirty || sprite->needs_redraw)) {
			if (self->cover_mask) {
				SPR_OBJ_OPS(sprite)->DrawMasked(sprite, self->main_canvas, self->cover_mask);

			} else {
				SPR_OBJ_OPS(sprite)->Render(sprite, self->main_canvas);
			}
		}

		if (sprite->dirty) {
			if (sprite->prev_visible && sprite->visible) {
				rect = sprite->region1.rect;
				rect = RectUnion(&rect, &sprite->region2.rect);

			} else if (sprite->prev_visible && !sprite->visible) {
				rect = sprite->region2.rect;

			} else if (sprite->visible) {
				rect = sprite->region1.rect;
			}

			RectIntersect(&rect, &bounds, &rect);
			SHAPE_BASE_OPS(region)->AddRect(rgn(region), &rect);
		}

		sprite->dirty = false;
		sprite->needs_redraw = false;
		sprite->region2 = sprite->region1;
		sprite->prev_visible = sprite->visible;
		sprite->prev_frame = sprite->frame;
		sprite->prev_flip_x = sprite->flip_x;
		sprite->prev_mask = sprite->mask_threshold;
		sprite->needs_redraw = false;
		sprite->dirty = sprite->needs_redraw;

		sprite = next_child(sprite);
	}
}

void view_surface_mark_hit(struct view_surface *self, struct rect_rgn_list *region)
{
	for (struct spr_obj *sprite = first_child(self); sprite;) {
		if (!sprite->dirty && sprite->visible) {
			if (SHAPE_BASE_OPS(region)->IntersectsRect(rgn(region), &sprite->region1.rect))
				sprite->needs_redraw = true;
		}

		sprite = next_child(sprite);
	}
}

void view_surface_remove_all_children(struct view_surface *self)
{
	struct list_node *head;

	do {
		head = self->children->head;

		if (head)
			VIEW_SURFACE_OPS(self)->RemoveChild(self, SprObjFromNode(head));

	} while (head);
}

struct rect ViewSurfaceGetBoundsV(const struct view_surface *self)
{
	return self->rect_a;
}

struct point ViewSurfaceGetOrigin(const struct view_surface *self)
{
	return self->origin;
}
