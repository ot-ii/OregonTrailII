#include "cliplabelview.h"

#include <stddef.h>

#include "../data/gfxdev.h"
#include "../draw/label.h"
#include "../base/memory.h"
#include "../draw/region.h"
#include "../data/strrsrc.h"

const struct clip_label_view_ops CLIP_LABEL_VIEW_OPS_TABLE = {
	{
		{
			{
				{clip_label_view_destroy},
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
	},
};

static struct shape *swap_shape(struct clip_label_view *self, struct shape *shape)
{
	struct clip_view *cv = &self->base;
	struct shape *old_shape = cv->shape;
	cv->shape = shape;
	ObjectDelete(cv->base.region_clip);

	if (cv->shape) {
		cv->base.region_clip = SHAPE_OPS(cv->shape)->MakeRegion(cv->shape);

	} else {
		cv->base.region_clip = RegionCreate();
	}

	ViewNotifyChanged(&cv->base);

	return old_shape;
}

static void init_base(struct clip_label_view *self, int32_t id, const struct rect *rect)
{
	ClipViewInit(&self->base, id, rect, NULL, false);
	self->base.base.base.base.ops = &CLIP_LABEL_VIEW_OPS_TABLE.base.base.base.base;
}

void ClipLabelViewInit(struct clip_label_view *self, int32_t id, const struct rect *rect, struct cstr *src,
	int32_t font, int32_t h_align, int32_t v_align, int32_t pen, int32_t brush, int32_t alt, int32_t pen_width,
	int32_t bg_mode)
{
	init_base(self, id, rect);

	struct rect bounds = *rect;
	struct label *text = LabelCreateStr(&bounds, src, font, h_align, v_align, pen, brush, alt, pen_width, bg_mode);
	swap_shape(self, &text->base);
}

void ClipLabelViewInitStrs(struct clip_label_view *self, int32_t id, const struct rect *rect,
	struct strrsrc *rsrc_src, int32_t font, int32_t h_align, int32_t v_align, int32_t pen, int32_t brush,
	int32_t alt, int32_t pen_width, int32_t bg_mode)
{
	init_base(self, id, rect);

	struct rect bounds = *rect;
	struct label *text = LabelCreateRsrc(&bounds, rsrc_src, font, h_align, v_align, pen, brush, alt, pen_width,
		bg_mode);
	swap_shape(self, &text->base);
}

void ClipLabelViewInitLabel(struct clip_label_view *self, int32_t id, const struct rect *rect, struct label *label)
{
	init_base(self, id, rect);

	swap_shape(self, label ? &label->base : NULL);
}

struct clip_label_view *ClipLabelViewCreate(int32_t id, const struct rect *rect, struct cstr *src, int32_t font,
	int32_t h_align, int32_t v_align, int32_t pen, int32_t brush, int32_t alt, int32_t pen_width, int32_t bg_mode)
{
	struct clip_label_view *self = MemAlloc(1, sizeof(struct clip_label_view));
	ClipLabelViewInit(self, id, rect, src, font, h_align, v_align, pen, brush, alt, pen_width, bg_mode);

	return self;
}

struct clip_label_view *ClipLabelViewCreateStrs(int32_t id, const struct rect *rect, struct strrsrc *rsrc_src,
	int32_t font, int32_t h_align, int32_t v_align, int32_t pen, int32_t brush, int32_t alt, int32_t pen_width,
	int32_t bg_mode)
{
	struct clip_label_view *self = MemAlloc(1, sizeof(struct clip_label_view));
	ClipLabelViewInitStrs(self, id, rect, rsrc_src, font, h_align, v_align, pen, brush, alt, pen_width, bg_mode);

	return self;
}

struct clip_label_view *ClipLabelViewCreateLabel(int32_t id, const struct rect *rect, struct label *label)
{
	struct clip_label_view *self = MemAlloc(1, sizeof(struct clip_label_view));
	ClipLabelViewInitLabel(self, id, rect, label);

	return self;
}

void ClipLabelViewDestroy(struct clip_label_view **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void clip_label_view_destroy(struct object *self)
{
	self->ops = &CLIP_VIEW_OPS_TABLE.base.base.base;
	clip_view_destroy(self);
}

struct view *CreateClipLabelView(struct ctrl_desc *desc)
{
	struct cstr *text = NULL;
	struct strrsrc *strings = NULL;
	int32_t item_index = desc->item_index;
	int32_t rsrc_id = desc->rsrc_id;
	bool owns_strings = false;
	struct rect rect = RectMake(desc->rect.top, desc->rect.left, desc->rect.bottom, desc->rect.right);
	struct clip_label_view *view;

	if (desc->src) {
		strings = desc->src;

	} else if (rsrc_id > 0) {
		strings = StrRsrcLoad(g_text_lib, rsrc_id, true);
		owns_strings = true;
	}

	if (item_index > -1) {
		if (strings)
			text = StrRsrcGetCStr(strings, item_index);

		if (owns_strings)
			StrRsrcDestroy(&strings);

		view = ClipLabelViewCreate(desc->id, &rect, text, desc->font_id, desc->margin_x, desc->margin_y,
			desc->pen_color, desc->brush_color, desc->alt_brush_color, 0, 1);

	} else {
		view = ClipLabelViewCreateStrs(desc->id, &rect, strings, desc->font_id, desc->margin_x, desc->margin_y,
			desc->pen_color, desc->brush_color, desc->alt_brush_color, 0, 1);
	}

	if ((desc->flags & 2) != 0)
		view->base.alt_draw = true;

	if (desc->cursor_id > 0)
		VIEW_OPS(view)->SetCursorId(&view->base.base, desc->cursor_id);

	return &view->base.base;
}

void ClipLabelViewSetLabelObj(struct clip_label_view *self, struct label *label)
{
	struct shape *old_shape = swap_shape(self, label ? &label->base : NULL);

	if (old_shape)
		ObjectDelete(old_shape);

	ViewNotifyChanged(&self->base.base);
}

void ClipLabelViewSetLabelAttr(struct clip_label_view *self, struct cstr *attr)
{
	struct label *label = (struct label *) self->base.shape;
	LABEL_OPS(label)->SetContent(label, attr);
	ViewNotifyChanged(&self->base.base);
}

void ClipLabelViewApplyLabel(struct clip_label_view *self, struct strrsrc *label)
{
	struct label *l = (struct label *) self->base.shape;
	LABEL_OPS(l)->SetSource(l, label);
	ViewNotifyChanged(&self->base.base);
}
