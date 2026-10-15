#include "clipview.h"

#include <stddef.h>

#include "../draw/dc.h"
#include "../base/memory.h"
#include "../draw/oval.h"
#include "../draw/region.h"
#include "../draw/roundrect.h"
#include "../draw/shape.h"

const struct clip_view_ops CLIP_VIEW_OPS_TABLE = {
	{
		{
			{clip_view_destroy},
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
		clip_view_draw_children,
		view_move_children,
	},
};

void ClipViewInit(struct clip_view *self, int32_t id, const struct rect *rect, struct shape *shape, bool alt_draw)
{
	ViewInit(&self->base, EVCLS_VIEW, id, rect);
	self->base.base.base.ops = &CLIP_VIEW_OPS_TABLE.base.base.base;

	if (shape) {
		ObjectDelete(self->base.region_clip);
		self->base.region_clip = SHAPE_OPS(shape)->MakeRegion(shape);
	}

	self->alt_draw = alt_draw;
	self->shape = shape;
}

struct clip_view *ClipViewCreate(int32_t id, const struct rect *rect, struct shape *shape, bool alt_draw)
{
	struct clip_view *self = MemAlloc(1, sizeof(struct clip_view));
	ClipViewInit(self, id, rect, shape, alt_draw);

	return self;
}

void ClipViewDestroy(struct clip_view **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void clip_view_destroy(struct object *self)
{
	struct clip_view *v = (struct clip_view *) self;

	if (v->shape)
		ObjectDestroy(&v->shape);

	self->ops = &VIEW_OPS_TABLE.base.base;
	view_destroy(self);
}

struct view *BuildRoundRectView(struct ctrl_desc *desc)
{
	struct rect rect = RectMake(desc->rect.top, desc->rect.left, desc->rect.bottom, desc->rect.right);
	int32_t pen_color = desc->pen_color;
	int32_t brush_color = desc->brush_color;
	int32_t alt_brush = desc->alt_brush_color;
	int32_t pen_width = desc->arg7;
	int32_t flags = desc->flags;
	struct shape *shape;

	switch (desc->style) {
		case 0:
			shape = ShapeCreateRect(&rect, pen_color, brush_color, alt_brush, pen_width, 0);
			break;
		case 2:
			shape = (struct shape *) OvalCreate(&rect, pen_color, brush_color, alt_brush, pen_width, 0);
			break;
		case 1:
			shape = (struct shape *) RoundRectCreate(&rect, desc->margin_y, desc->margin_x, pen_color,
				brush_color, alt_brush, pen_width, 0);
			break;
		default:
			shape = ShapeCreateRect(&rect, pen_color, brush_color, alt_brush, pen_width, 0);
			break;
	}

	if ((flags & 4) != 0) {
		if (shape->penStyle != 1)
			shape->penStyle = 1;
	}

	struct clip_view *view = ClipViewCreate(desc->id, &rect, shape, false);

	if ((flags & 2) != 0)
		view->alt_draw = true;

	if (desc->cursor_id > 0)
		VIEW_OPS(view)->SetCursorId(&view->base, desc->cursor_id);

	return &view->base;
}

void clip_view_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds)
{
	struct clip_view *v = (struct clip_view *) self;
	struct rect clipped = self->rect_b;

	if (v->shape) {
		struct dc dc2;
		DcInit(&dc2, dc);

		struct region *saved_region = self->region_bounds;
		struct rect empty;
		RectIntersect(&clipped, bounds, &empty);
		self->region_bounds = RegionCreateRect(&clipped, 1, 0, 0);
		ViewComputeClip(self, dc);

		if (v->alt_draw) {
			SHAPE_OPS(v->shape)->DrawBrush20(v->shape, dc);

		} else {
			SHAPE_OPS(v->shape)->Draw(v->shape, dc);
		}

		ObjectDelete(self->region_bounds);
		self->region_bounds = saved_region;

		dc_destroy(&dc2.base);
	}

	view_draw_children(self, dc, bounds);
}
