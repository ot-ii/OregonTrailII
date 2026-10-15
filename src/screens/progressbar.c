#include "progressbar.h"

#include <stddef.h>

#include "../base/memory.h"
#include "../ui/polyctrl.h"
#include "../draw/shape.h"

const struct progress_bar_ops PROGRESS_BAR_OPS_TABLE = {
	{
		{
			{
				{progress_bar_destroy},
				evt_handler_is_handled,
				evt_handler_set_handled_for,
				evt_handler_set_handled,
				evt_handler_clear_handled_for,
				evt_handler_clear_handled,
				pic_view_on_event,
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
			progress_bar_draw_children,
			view_move_children,
		},
	},
};

void ProgressBarInit(struct progress_bar *self, int32_t id, const struct point *pos, int32_t img_id)
{
	PicViewInitPoint(&self->base, id, pos, img_id);
	self->base.base.base.base.ops = &PROGRESS_BAR_OPS_TABLE.base.base.base.base;

	self->fill = NULL;
}

struct progress_bar *ProgressBarCreate(int32_t id, const struct point *pos, int32_t img_id)
{
	struct progress_bar *self = MemAlloc(1, sizeof(struct progress_bar));
	ProgressBarInit(self, id, pos, img_id);

	return self;
}

void ProgressBarDestroy(struct progress_bar **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void progress_bar_destroy(struct object *obj)
{
	struct progress_bar *self = (struct progress_bar *) obj;

	if (self->fill)
		ObjectDelete(self->fill);

	obj->ops = &PIC_VIEW_OPS_TABLE.base.base.base;
	pic_view_destroy(obj);
}

void progress_bar_draw_children(struct view *view, struct screen_dc *dc, const struct rect *bounds)
{
	struct progress_bar *self = (struct progress_bar *) view;

	pic_view_draw_children(view, dc, bounds);

	if (self->fill)
		SHAPE_OPS(self->fill)->DrawBrush20(self->fill, dc);
}

void ProgressBarSetMarker(struct progress_bar *self, int32_t pct)
{
	struct point start = PointMake(12, 34);
	struct point end = PointMake(12, 34);

	int32_t offset = pct / 3;

	if (offset > 27)
		offset = 27;

	if (offset < 0)
		offset = 0;

	end.v = end.v - offset;

	if (self->fill)
		ObjectDelete(self->fill);

	struct point origin = ViewGetOrigin(&self->base.base);
	start.h = start.h + origin.h;
	start.v = start.v + origin.v;

	origin = ViewGetOrigin(&self->base.base);
	end.h = end.h + origin.h;
	end.v = end.v + origin.v;

	self->fill = &LineCtrlCreate(&start, &end, 139, 139, 0, 2)->base.base;
	ViewNotifyChanged(&self->base.base);
}
