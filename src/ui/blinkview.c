#include "blinkview.h"

#include <stddef.h>

#include "../draw/canvas.h"
#include "../base/memory.h"
#include "../draw/viewsurface.h"
#include "window.h"

const struct blink_view_ops BLINK_VIEW_OPS_TABLE = {
	{
		{
			{blink_view_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			view_on_event,
			blink_view_on_idle,
			blink_view_on_idle_spare,
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
		blink_view_draw_children,
		view_move_children,
	},
};

static void init_base(struct blink_view *self, int32_t id, const struct rect *rect)
{
	ViewInit(&self->base, EVCLS_PICVIEW, id, rect);
	self->base.base.base.ops = &BLINK_VIEW_OPS_TABLE.base.base.base;
}

void BlinkViewInit(struct blink_view *self, int32_t id, const struct rect *rect, bool phase)
{
	init_base(self, id, rect);

	self->surface = NULL;
	self->phase = phase;
	EvtHandlerAddIdleHandler(&self->base.base, phase, 0);
}

void BlinkViewInitSurface(struct blink_view *self, int32_t id, struct rect *rect, struct palette *palette,
	bool has_overlay, bool has_mask, bool phase)
{
	init_base(self, id, rect);

	self->surface = ViewSurfaceCreate(rect, palette, has_overlay, has_mask);
	self->phase = phase;
	EvtHandlerAddIdleHandler(&self->base.base, phase, 0);
}

struct blink_view *BlinkViewCreate(int32_t id, const struct rect *rect, bool phase)
{
	struct blink_view *self = MemAlloc(1, sizeof(struct blink_view));
	BlinkViewInit(self, id, rect, phase);

	return self;
}

struct blink_view *BlinkViewCreateSurface(int32_t id, struct rect *rect, struct palette *palette, bool has_overlay,
	bool has_mask, bool phase)
{
	struct blink_view *self = MemAlloc(1, sizeof(struct blink_view));
	BlinkViewInitSurface(self, id, rect, palette, has_overlay, has_mask, phase);

	return self;
}

void BlinkViewDestroy(struct blink_view **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void blink_view_destroy(struct object *self)
{
	struct blink_view *v = (struct blink_view *) self;

	ObjectDestroy(&v->surface);

	self->ops = &VIEW_OPS_TABLE.base.base;
	view_destroy(self);
}

void blink_view_on_idle_spare(struct evt_handler *self, void *task)
{
	struct blink_view *v = (struct blink_view *) self;

	if (!v->phase) {
		if (v->base.owner_window) {
			if (ViewIsVisible(&v->base))
				VIEW_SURFACE_OPS(v->surface)->RenderPipeline(v->surface, v->base.owner_window->windc, true);
		}
	}
}

void blink_view_on_idle(struct evt_handler *self, void *task)
{
	struct blink_view *v = (struct blink_view *) self;

	if (v->phase) {
		if (v->base.owner_window) {
			if (ViewIsVisible(&v->base))
				VIEW_SURFACE_OPS(v->surface)->RenderPipeline(v->surface, v->base.owner_window->windc, true);
		}
	}
}

void blink_view_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds)
{
	struct blink_view *v = (struct blink_view *) self;

	if (ViewIsVisible(self)) {
		VIEW_SURFACE_OPS(v->surface)->MarkAllDirty(v->surface);
		VIEW_SURFACE_OPS(v->surface)->RenderPipeline(v->surface, NULL, false);

		struct canvas *canvas = v->surface->main_canvas;
		struct point origin = ViewSurfaceGetOrigin(v->surface);
		CANVAS_OPS(canvas)->DrawAt(canvas, dc, &origin, 0);
	}
}
