#include "textview.h"

#include <stddef.h>

#include "../data/gfxdev.h"
#include "../base/memory.h"
#include "../draw/region.h"
#include "../data/strrsrc.h"
#include "../draw/text.h"

const struct text_view_ops TEXT_VIEW_OPS_TABLE = {
	{
		{
			{
				{text_view_destroy},
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

static struct shape *swap_shape(struct text_view *self, struct shape *shape)
{
	struct clip_view *cv = &self->base;
	struct shape *prev = cv->shape;
	cv->shape = shape;
	ObjectDelete(cv->base.region_clip);

	if (cv->shape) {
		cv->base.region_clip = SHAPE_OPS(cv->shape)->MakeRegion(cv->shape);

	} else {
		cv->base.region_clip = RegionCreate();
	}

	ViewNotifyChanged(&cv->base);

	return prev;
}

static void init_base(struct text_view *self, int32_t id, const struct rect *rect)
{
	ClipViewInit(&self->base, id, rect, NULL, false);
	self->base.base.base.base.ops = &TEXT_VIEW_OPS_TABLE.base.base.base.base;
}

void TextViewInit(struct text_view *self, int32_t id, const struct rect *rect, struct cstr *src, int32_t font_id,
	int32_t h_align, int32_t v_align, int32_t linespacing, int32_t tabwidth, int32_t pen, int32_t brush, int32_t alt,
	int32_t pen_width, int32_t bg_mode)
{
	init_base(self, id, rect);

	struct rect bounds = *rect;
	struct text *text = TextCreateStr(&bounds, src, font_id, h_align, v_align, linespacing, tabwidth, pen, brush,
		alt, pen_width, bg_mode);
	swap_shape(self, &text->base.base);
}

void TextViewInitStrs(struct text_view *self, int32_t id, const struct rect *rect, struct strrsrc *rsrc_src,
	int32_t font_id, int32_t h_align, int32_t v_align, int32_t linespacing, int32_t tabwidth, int32_t pen,
	int32_t brush, int32_t alt, int32_t pen_width, int32_t bg_mode)
{
	init_base(self, id, rect);

	struct rect bounds = *rect;
	struct text *text = TextCreateRsrc(&bounds, rsrc_src, font_id, h_align, v_align, linespacing, tabwidth, pen,
		brush, alt, pen_width, bg_mode);
	swap_shape(self, &text->base.base);
}

void TextViewInitText(struct text_view *self, int32_t id, const struct rect *rect, struct text *text)
{
	init_base(self, id, rect);

	swap_shape(self, text ? &text->base.base : NULL);
}

struct text_view *TextViewCreate(int32_t id, const struct rect *rect, struct cstr *src, int32_t font_id,
	int32_t h_align, int32_t v_align, int32_t linespacing, int32_t tabwidth, int32_t pen, int32_t brush, int32_t alt,
	int32_t pen_width, int32_t bg_mode)
{
	struct text_view *self = MemAlloc(1, sizeof(struct text_view));
	TextViewInit(self, id, rect, src, font_id, h_align, v_align, linespacing, tabwidth, pen, brush, alt, pen_width,
		bg_mode);

	return self;
}

struct text_view *TextViewCreateStrs(int32_t id, const struct rect *rect, struct strrsrc *rsrc_src, int32_t font_id,
	int32_t h_align, int32_t v_align, int32_t linespacing, int32_t tabwidth, int32_t pen, int32_t brush, int32_t alt,
	int32_t pen_width, int32_t bg_mode)
{
	struct text_view *self = MemAlloc(1, sizeof(struct text_view));
	TextViewInitStrs(self, id, rect, rsrc_src, font_id, h_align, v_align, linespacing, tabwidth, pen, brush, alt,
		pen_width, bg_mode);

	return self;
}

struct text_view *TextViewCreateText(int32_t id, const struct rect *rect, struct text *text)
{
	struct text_view *self = MemAlloc(1, sizeof(struct text_view));
	TextViewInitText(self, id, rect, text);

	return self;
}

void TextViewDestroy(struct text_view **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void text_view_destroy(struct object *self)
{
	self->ops = &CLIP_VIEW_OPS_TABLE.base.base.base;
	clip_view_destroy(self);
}

struct view *CreateTextView(struct ctrl_desc *desc)
{
	struct cstr *line = NULL;
	struct strrsrc *strings = NULL;
	int32_t item_index = desc->item_index;
	int32_t rsrc_id = desc->rsrc_id;
	bool owns_strings = false;
	struct rect rect = RectMake(desc->rect.top, desc->rect.left, desc->rect.bottom, desc->rect.right);
	struct text_view *view;

	if (desc->src) {
		strings = desc->src;

	} else if (rsrc_id > 0) {
		strings = StrRsrcLoad(g_text_lib, rsrc_id, true);
		owns_strings = true;
	}

	if (item_index > -1) {
		if (strings)
			line = StrRsrcGetCStr(strings, item_index);

		if (owns_strings)
			StrRsrcDestroy(&strings);

		view = TextViewCreate(desc->id, &rect, line, desc->font_id, desc->margin_x, desc->margin_y, desc->style, 0,
			desc->pen_color, desc->brush_color, desc->alt_brush_color, 0, 1);

	} else {
		view = TextViewCreateStrs(desc->id, &rect, strings, desc->font_id, desc->margin_x, desc->margin_y,
			desc->style, 0, desc->pen_color, desc->brush_color, desc->alt_brush_color, 0, 1);
	}

	if ((desc->flags & 2) != 0) {
		view->base.alt_draw = true;
		view->base.shape->penWidth = 1;
	}

	if (desc->cursor_id > 0)
		VIEW_OPS(view)->SetCursorId(&view->base.base, desc->cursor_id);

	return &view->base.base;
}

void TextViewSetSourceObj(struct text_view *self, struct text *text)
{
	struct shape *prev_shape = swap_shape(self, text ? &text->base.base : NULL);

	if (prev_shape)
		ObjectDelete(prev_shape);

	ViewNotifyChanged(&self->base.base);
}

void TextViewSetSource(struct text_view *self, struct cstr *src)
{
	struct label *text = (struct label *) self->base.shape;
	LABEL_OPS(text)->SetContent(text, src);
	ViewNotifyChanged(&self->base.base);
}

void TextViewSetText(struct text_view *self, struct strrsrc *text)
{
	struct label *t = (struct label *) self->base.shape;
	LABEL_OPS(t)->SetSource(t, text);
	ViewNotifyChanged(&self->base.base);
}
