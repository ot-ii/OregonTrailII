#include "mediaview.h"

#include <stddef.h>

#include "../draw/canvas.h"
#include "../draw/dc.h"
#include "../draw/drawable.h"
#include "../base/memory.h"
#include "../draw/palette.h"
#include "../draw/pictserver.h"
#include "../draw/region.h"

const struct media_view_ops MEDIA_VIEW_OPS_TABLE = {
	{
		{
			{media_view_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			view_on_event,
			view_on_idle,
			view_on_idle_spare,
		},
		view_add_child,
		view_get_bounds,
		view_get_clip_region,
		view_hide,
		view_remove_child,
		view_remove_from_parent,
		view_set_cursor_id,
		view_set_device,
		view_show,
		media_view_draw_children,
		view_move_children,
	},
};

static struct drawable *acquire(int32_t id)
{
	return ServerCreateVirtual(&g_pict_server->base.base, id, true);
}

void MediaViewInit(struct media_view *self, int32_t id, const struct point *pos, int32_t source_id, int32_t bg_id,
	const struct point *anchor_arg, int32_t fg_id)
{
	struct view *v = &self->base;

	ViewInitPoint(v, EVCLS_PICVIEW, id, pos);
	v->base.base.ops = &MEDIA_VIEW_OPS_TABLE.base.base.base;

	struct point size = {0};

	self->anchor = *anchor_arg;
	self->source = NULL;
	self->composite = NULL;

	self->foreground = acquire(fg_id);
	self->background = acquire(bg_id);

	if (self->background)
		OBJ_BASE_OPS(self->background)->GetSize(&self->background->base, &size);

	v->rect_a.right = v->rect_a.left + size.h;
	v->rect_a.bottom = v->rect_a.top + size.v;
	v->rect_b.right = v->rect_b.left + size.h;
	v->rect_b.bottom = v->rect_b.top + size.v;

	RegionDestroy(&v->region_bounds);
	v->region_bounds = RegionCreateRect(&v->rect_b, 1, 0, 0);

	RegionDestroy(&v->region_clip);
	struct rect bounds = VIEW_OPS(v)->GetBounds(v);
	v->region_clip = RegionCreateRect(&bounds, 1, 0, 0);

	MediaViewLoadSource(self, source_id);
}

struct media_view *MediaViewCreate(int32_t id, const struct point *pos, int32_t source_id, int32_t bg_id,
	const struct point *anchor_arg, int32_t fg_id)
{
	struct media_view *self = MemAlloc(1, sizeof(struct media_view));
	MediaViewInit(self, id, pos, source_id, bg_id, anchor_arg, fg_id);

	return self;
}

void MediaViewDestroy(struct media_view **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void media_view_destroy(struct object *obj)
{
	struct media_view *self = (struct media_view *) obj;

	if (self->source)
		ReleaseDrawable(&self->source->base.base, false);

	if (self->background)
		ReleaseDrawable(&self->background->base.base, false);

	if (self->foreground)
		ReleaseDrawable(&self->foreground->base.base, false);

	if (self->composite)
		CanvasDestroy(&self->composite);

	obj->ops = &VIEW_OPS_TABLE.base.base;
	view_destroy(obj);
}

struct view *MediaViewFactory(struct ctrl_desc *desc)
{
	struct point pos = PointMake(desc->rect.left, desc->rect.top);
	struct point anchor = PointMake(desc->margin_x, desc->margin_y);

	struct media_view *view = MediaViewCreate(desc->id, &pos, desc->obj_type, desc->style, &anchor, desc->arg7);

	if (!view->source) {
		MediaViewDestroy(&view);

	} else if (desc->cursor_id > 0) {
		VIEW_OPS(view)->SetCursorId(&view->base, desc->cursor_id);
	}

	return (struct view *) view;
}

void media_view_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds)
{
	struct media_view *mv = (struct media_view *) self;

	if (mv->composite) {
		struct dc clip_dc;
		DcInit(&clip_dc, dc);
		ViewComputeClip(self, dc);
		SHAPE_OPS(mv->composite)->Draw(&mv->composite->base, dc);
		dc_destroy(&clip_dc.base);
	}

	view_draw_children(self, dc, bounds);
}

static struct rect at_anchor(const struct rect *r, const struct point *anchor)
{
	int32_t width = r->right - r->left;
	int32_t height = r->bottom - r->top;

	return RectMake(anchor->v, anchor->h, anchor->v + height, anchor->h + width);
}

static struct rect compose(struct media_view *self, struct rect *src_rect)
{
	struct point size = {0};
	struct rect bg_rect = {0};
	struct rect fg_rect = {0};

	struct obj_base *source = &self->source->base;
	OBJ_BASE_OPS(source)->GetSize(source, &size);
	struct canvas *src_canvas = OBJ_BASE_OPS(source)->GetRect(source, src_rect);
	struct rect dest_rect = at_anchor(src_rect, &self->anchor);

	struct obj_base *background = &self->background->base;
	struct canvas *bg_canvas = OBJ_BASE_OPS(background)->GetRect(background, &bg_rect);
	struct obj_base *foreground = &self->foreground->base;
	struct canvas *fg_canvas = OBJ_BASE_OPS(foreground)->GetRect(foreground, &fg_rect);

	self->composite = CanvasCreateRect(src_rect, 8, 0xFF);
	CANVAS_OPS(self->composite)->StretchBlit(self->composite, fg_canvas, &dest_rect, src_rect, 0, NULL);

	struct point pos = RectTopLeft(src_rect);
	CANVAS_OPS(self->composite)->AlphaBlend(self->composite, src_canvas, bg_canvas, &pos, src_rect, 0, src_rect,
		g_palette);

	return dest_rect;
}

void MediaViewLoadSource(struct media_view *self, int32_t source_id)
{
	struct rect src_rect = {0};

	if (self->source)
		ReleaseDrawable(&self->source->base.base, false);

	if (self->composite)
		CanvasDestroy(&self->composite);

	self->source = acquire(source_id);

	if (!self->source || !self->background || !self->foreground)
		return;

	struct rect dest_rect = compose(self, &src_rect);

	dest_rect.right = dest_rect.right - dest_rect.left;
	dest_rect.bottom = dest_rect.bottom - dest_rect.top;
	dest_rect.left = dest_rect.top = 0;
	struct point origin = RectTopLeft(&self->base.rect_b);
	RectOffset(&dest_rect, origin.h, origin.v);

	SHAPE_OPS(self->composite)->SetRect(&self->composite->base, &dest_rect);
	ViewNotifyChanged(&self->base);
}

void MediaViewSetBackground(struct media_view *self, int32_t bg_id)
{
	struct rect src_rect = {0};

	if (self->background)
		ReleaseDrawable(&self->background->base.base, false);

	if (self->composite)
		CanvasDestroy(&self->composite);

	self->background = acquire(bg_id);

	if (!self->source || !self->background || !self->foreground)
		return;

	struct rect dest_rect = compose(self, &src_rect);

	struct point origin = RectTopLeft(&self->base.rect_b);
	RectOffset(&dest_rect, origin.h, origin.v);

	SHAPE_OPS(self->composite)->SetRect(&self->composite->base, &dest_rect);
	ViewNotifyChanged(&self->base);
}
