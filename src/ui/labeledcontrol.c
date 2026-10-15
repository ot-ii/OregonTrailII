#include "labeledcontrol.h"

#include <stddef.h>

#include "../data/gfxdev.h"
#include "../draw/label.h"
#include "../base/memory.h"
#include "../draw/screendc.h"
#include "../data/strrsrc.h"

const struct labeled_control_ops LABELED_CONTROL_OPS_TABLE = {
	{
		{
			{labeled_control_destroy},
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
		labeled_control_draw_children,
		view_move_children,
	},
};

void LabeledControlInit(struct labeled_control *self, int32_t id, const struct rect *src, struct cstr *content,
	int32_t font, int32_t h_align, int32_t fill, int32_t line, int32_t style)
{
	ViewInit(&self->base, EVCLS_LABELED_CONTROL, id, src);
	self->base.base.base.ops = &LABELED_CONTROL_OPS_TABLE.base.base.base;

	struct rect rect = *src;
	self->label_x = src->top;
	self->label_y = src->left;
	rect.left = self->label_y + 8;
	rect.right = src->right - 8;
	self->style = style;
	self->pen_width = 1;
	self->fill_color = fill;
	self->line_color = line;
	self->has_label = content != NULL;
	self->label = LabelCreateStr(&rect, content, font, h_align, 0, 0, self->fill_color, self->line_color, 0, 0);
}

struct labeled_control *LabeledControlCreate(int32_t id, const struct rect *src, struct cstr *content, int32_t font,
	int32_t h_align, int32_t fill, int32_t line, int32_t style)
{
	struct labeled_control *self = MemAlloc(1, sizeof(struct labeled_control));
	LabeledControlInit(self, id, src, content, font, h_align, fill, line, style);

	return self;
}

void LabeledControlDestroy(struct labeled_control **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void labeled_control_destroy(struct object *self)
{
	struct labeled_control *v = (struct labeled_control *) self;

	ObjectDestroy(&v->label);

	self->ops = &VIEW_OPS_TABLE.base.base;
	view_destroy(self);
}

struct view *CreateLabeledControlFromTemplate(struct ctrl_desc *desc)
{
	struct rect rect = RectMake(desc->rect.top, desc->rect.left, desc->rect.bottom, desc->rect.right);
	struct cstr *label_str = NULL;
	int32_t item_index = desc->item_index;

	if (item_index > -1) {
		if (desc->src) {
			label_str = StrRsrcGetCStr(desc->src, item_index);

		} else if (desc->rsrc_id > 0) {
			struct strrsrc *strings = StrRsrcLoad(g_text_lib, desc->rsrc_id, true);

			if (strings)
				label_str = StrRsrcGetCStr(strings, item_index);

			StrRsrcDestroy(&strings);
		}
	}

	struct labeled_control *control = LabeledControlCreate(desc->id, &rect, label_str, desc->font_id,
		desc->margin_x, desc->brush_color, desc->alt_brush_color, desc->style);

	if (desc->cursor_id > 0)
		VIEW_OPS(control)->SetCursorId(&control->base, desc->cursor_id);

	return &control->base;
}

static void move_label(struct label *label, int32_t label_y, int32_t label_x)
{
	struct shape *shape = &label->base;
	struct rect label_rect = SHAPE_OPS(shape)->GetRectByVal(shape);
	int32_t width = label_rect.right - label_rect.left;
	int32_t height = label_rect.bottom - label_rect.top;
	label_rect.top = label_x;
	label_rect.left = label_y;
	label_rect.right = width + label_y;
	label_rect.bottom = height + label_x;
	SHAPE_OPS(shape)->SetRect(shape, &label_rect);
}

void labeled_control_draw_children(struct view *self, struct screen_dc *dc, const struct rect *invalid_rect)
{
	struct labeled_control *v = (struct labeled_control *) self;
	struct rect frame_rect = self->rect_b;

	ScreenDcSetPenWidth2(dc, v->pen_width);
	SCREEN_DC_OPS(dc)->SetBkColor(dc, v->line_color);
	SCREEN_DC_OPS(dc)->SetColor(dc, v->fill_color);

	if (v->has_label) {
		if (self->rect_b.top != v->label_x || self->rect_b.left != v->label_y) {
			v->label_x = self->rect_b.top;
			v->label_y = self->rect_b.left + 8;
			move_label(v->label, v->label_y, v->label_x);
		}

		int32_t metrics[4];
		ScreenDcGetTextMetrics(dc, metrics, v->label->font);
		frame_rect.top = (metrics[0] + metrics[1]) / 2 + frame_rect.top;
	}

	ScreenDcFrameRect(dc, &frame_rect, 0);

	if (v->has_label)
		SHAPE_OPS(v->label)->DrawBrush20(&v->label->base, dc);

	view_draw_children(self, dc, invalid_rect);
}

void LabeledControlSetLineColor(struct labeled_control *self, int32_t line_color)
{
	self->line_color = line_color;
	self->label->base.altBrushColor = self->line_color;
}

void LabeledControlSetFillColor(struct labeled_control *self, int32_t fill_color)
{
	self->fill_color = fill_color;
	self->label->base.brushColor = self->fill_color;
}

void LabeledControlSetTitle(struct labeled_control *self, struct cstr *title)
{
	LABEL_OPS(self->label)->SetContent(self->label, title);
	self->has_label = title != NULL;
	ViewNotifyChanged(&self->base);
}
