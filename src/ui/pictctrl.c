#include "pictctrl.h"

#include <stddef.h>

#include "../draw/canvas.h"
#include "../base/cstr.h"
#include "../draw/dispobj.h"
#include "../data/gfxdev.h"
#include "../base/memory.h"
#include "../draw/region.h"
#include "../draw/shape.h"
#include "../draw/statefuldispobj.h"
#include "../data/strrsrc.h"
#include "../draw/text.h"
#include "../draw/textlabel.h"

const struct pict_ctrl_ops PICT_CTRL_OPS_TABLE = {
	{
		{
			{
				{
					{pict_ctrl_destroy},
					evt_handler_is_handled,
					control_set_handled_for,
					control_set_handled,
					control_clear_handled_for,
					control_clear_handled,
					button_on_event,
					view_on_idle,
					view_on_idle_spare,
				},
				view_add_child,
				button_get_bounds,
				view_get_clip_region,
				button_hide,
				view_remove_child,
				view_remove_from_parent,
				view_set_cursor_id,
				control_set_device,
				button_show,
				button_draw_children,
				view_move_children,
			},
			button_set_value,
			control_on_command,
			control_focus_sibling,
			button_on_key_down,
			control_set_focus,
			button_create_or_destroy,
		},
		button_on_char,
		button_on_button_down,
		button_on_button_up,
		button_on_mouse_move,
	},
};

static void set_data(struct pict_ctrl *self, struct disp_obj *data)
{
	struct button *button = &self->base;
	struct view *view = &button->base.base;

	button->data_src = data;
	ObjectDelete(view->region_clip);

	if (button->data_src) {
		struct rect b = VIEW_OPS(view)->GetBounds(view);
		struct point top_left = RectTopLeft(&b);
		struct text_label *label = (struct text_label *) button->data_src;
		view->region_clip = TEXT_LABEL_OPS(label)->MakeRegion(label, &top_left);

	} else {
		view->region_clip = RegionCreate();
	}
}

static void adjust_rect(struct rect *rect, const struct point *size)
{
	int32_t right = size->h + rect->left;
	int32_t bottom = size->v + rect->top;
	rect->right = right;
	rect->bottom = bottom;
}

void PictCtrlInit(struct pict_ctrl *self, int32_t id, const struct event *event_src, const struct point *pos,
	int32_t obj_type, int32_t style, int32_t value, struct cstr *source, int32_t font_id, int32_t margin_x,
	int32_t margin_y)
{
	ButtonInitDispObjPoint(&self->base, id, event_src, NULL, pos, style, value);
	self->base.base.base.base.base.ops = &PICT_CTRL_OPS_TABLE.base.base.base.base.base;

	struct view *view = &self->base.base.base;
	struct point size = PointMake(0, 0);

	struct stateful_disp_obj *obj = StatefulDispObjCreate(obj_type, source, &size, font_id, margin_x, margin_y);
	set_data(self, (struct disp_obj *) obj);

	struct text_label *label = &obj->base;
	struct rect height_bounds = TEXT_LABEL_OPS(label)->GetBounds(label);
	struct rect width_bounds = TEXT_LABEL_OPS(label)->GetBounds(label);
	int32_t height = height_bounds.bottom - height_bounds.top;
	int32_t width = width_bounds.right - width_bounds.left;
	size.h = width;
	size.v = height;

	adjust_rect(&view->rect_a, &size);
	adjust_rect(&view->rect_b, &size);

	ObjectDelete(view->region_bounds);
	view->region_bounds = RegionCreateRect(&view->rect_b, 1, 0, 0);
}

struct pict_ctrl *PictCtrlCreate(int32_t id, const struct event *event_src, const struct point *pos,
	int32_t obj_type, int32_t style, int32_t value, struct cstr *source, int32_t font_id, int32_t margin_x,
	int32_t margin_y)
{
	struct pict_ctrl *self = MemAlloc(1, sizeof(struct pict_ctrl));
	PictCtrlInit(self, id, event_src, pos, obj_type, style, value, source, font_id, margin_x, margin_y);

	return self;
}

void PictCtrlDestroy(struct pict_ctrl **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void pict_ctrl_destroy(struct object *self)
{
	self->ops = &BUTTON_OPS_TABLE.base.base.base.base;
	button_destroy(self);
}

struct view *CtrlFactoryPicControl(struct ctrl_desc *desc)
{
	struct cstr *label = NULL;
	int32_t item_index = desc->item_index;
	struct rect rect = RectMake(desc->rect.top, desc->rect.left, desc->rect.bottom, desc->rect.right);

	if (item_index > -1) {
		if (desc->src) {
			label = StrRsrcGetCStr(desc->src, item_index);

		} else if (desc->rsrc_id > 0) {
			struct strrsrc *strings = StrRsrcLoad(g_text_lib, desc->rsrc_id, true);

			label = strings ? StrRsrcGetCStr(strings, item_index) : NULL;
			StrRsrcDestroy(&strings);
		}
	}

	struct point top_left = RectTopLeft(&rect);
	struct pict_ctrl *ctrl = PictCtrlCreate(desc->id, desc->event, &top_left, desc->obj_type, desc->style,
		desc->value, label, desc->font_id, desc->margin_x, desc->margin_y);

	struct text_label *obj = (struct text_label *) ctrl->base.data_src;
	obj->color_mode1 = (int16_t) desc->brush_color;

	int32_t color = (int16_t) desc->alt_brush_color;
	obj->pen_color = color;

	if (obj->text)
		obj->text->base.base.penColor = color;

	obj->color_default = (int16_t) desc->alt_brush_color;
	obj->color_mode2 = (int16_t) desc->pen_color;

	if (desc->flags & 2)
		obj->align = 1;

	ControlApplyDescriptor(&ctrl->base.base, desc);

	return &ctrl->base.base.base;
}


const struct pict_check_ops PICT_CHECK_OPS_TABLE = {
	{
		{
			{
				{
					{
						{pict_check_destroy},
						evt_handler_is_handled,
						control_set_handled_for,
						control_set_handled,
						control_clear_handled_for,
						control_clear_handled,
						button_on_event,
						view_on_idle,
						view_on_idle_spare,
					},
					view_add_child,
					button_get_bounds,
					view_get_clip_region,
					button_hide,
					view_remove_child,
					view_remove_from_parent,
					view_set_cursor_id,
					control_set_device,
					button_show,
					button_draw_children,
					view_move_children,
				},
				button_set_value,
				control_on_command,
				control_focus_sibling,
				button_on_key_down,
				control_set_focus,
				button_create_or_destroy,
			},
			button_on_char,
			button_on_button_down,
			button_on_button_up,
			button_on_mouse_move,
		},
	},
};

void PictCheckInit(struct pict_check *self, int32_t id, const struct event *event_src, const struct point *pos,
	int32_t obj_type, int32_t value, struct cstr *source, int32_t font_id)
{
	PictCtrlInit(&self->base, id, event_src, pos, obj_type, 2, value, source, font_id, 0, 0);
	self->base.base.base.base.base.base.ops = &PICT_CHECK_OPS_TABLE.base.base.base.base.base.base;
}

struct pict_check *PictCheckCreate(int32_t id, const struct event *event_src, const struct point *pos,
	int32_t obj_type, int32_t value, struct cstr *source, int32_t font_id)
{
	struct pict_check *self = MemAlloc(1, sizeof(struct pict_check));
	PictCheckInit(self, id, event_src, pos, obj_type, value, source, font_id);

	return self;
}

void PictCheckDestroy(struct pict_check **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void pict_check_destroy(struct object *self)
{
	self->ops = &PICT_CTRL_OPS_TABLE.base.base.base.base.base;
	pict_ctrl_destroy(self);
}


const struct pict_radio_ops PICT_RADIO_OPS_TABLE = {
	{
		{
			{
				{
					{
						{pict_radio_destroy},
						evt_handler_is_handled,
						control_set_handled_for,
						control_set_handled,
						control_clear_handled_for,
						control_clear_handled,
						button_on_event,
						view_on_idle,
						view_on_idle_spare,
					},
					view_add_child,
					button_get_bounds,
					view_get_clip_region,
					button_hide,
					view_remove_child,
					view_remove_from_parent,
					view_set_cursor_id,
					control_set_device,
					button_show,
					button_draw_children,
					view_move_children,
				},
				button_set_value,
				control_on_command,
				control_focus_sibling,
				button_on_key_down,
				control_set_focus,
				button_create_or_destroy,
			},
			button_on_char,
			button_on_button_down,
			button_on_button_up,
			button_on_mouse_move,
		},
	},
};

void PictRadioInit(struct pict_radio *self, int32_t id, const struct event *event_src, const struct point *pos,
	int32_t obj_type, int32_t value, struct cstr *source, int32_t font_id)
{
	PictCtrlInit(&self->base, id, event_src, pos, obj_type, 1, value, source, font_id, 0, 0);
	self->base.base.base.base.base.base.ops = &PICT_RADIO_OPS_TABLE.base.base.base.base.base.base;
}

struct pict_radio *PictRadioCreate(int32_t id, const struct event *event_src, const struct point *pos,
	int32_t obj_type, int32_t value, struct cstr *source, int32_t font_id)
{
	struct pict_radio *self = MemAlloc(1, sizeof(struct pict_radio));
	PictRadioInit(self, id, event_src, pos, obj_type, value, source, font_id);

	return self;
}

void PictRadioDestroy(struct pict_radio **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void pict_radio_destroy(struct object *self)
{
	self->ops = &PICT_CTRL_OPS_TABLE.base.base.base.base.base;
	pict_ctrl_destroy(self);
}


const struct scroll_ctrl_ops SCROLL_CTRL_OPS_TABLE = {
	{
		{
			{
				{
					{
						{scroll_ctrl_destroy},
						evt_handler_is_handled,
						control_set_handled_for,
						control_set_handled,
						control_clear_handled_for,
						control_clear_handled,
						button_on_event,
						view_on_idle,
						view_on_idle_spare,
					},
					view_add_child,
					button_get_bounds,
					view_get_clip_region,
					button_hide,
					view_remove_child,
					view_remove_from_parent,
					view_set_cursor_id,
					control_set_device,
					button_show,
					scroll_ctrl_draw_children,
					view_move_children,
				},
				button_set_value,
				control_on_command,
				control_focus_sibling,
				button_on_key_down,
				control_set_focus,
				button_create_or_destroy,
			},
			button_on_char,
			button_on_button_down,
			button_on_button_up,
			button_on_mouse_move,
		},
	},
};

void ScrollCtrlInit(struct scroll_ctrl *self, int32_t type, const struct event *event_src, const struct point *pos,
	int32_t obj_type, struct canvas *scrollbar)
{
	PictCtrlInit(&self->base, type, event_src, pos, obj_type, 0, 0, NULL, 0, 0, 0);
	self->base.base.base.base.base.base.ops = &SCROLL_CTRL_OPS_TABLE.base.base.base.base.base.base;

	self->scrollbar = scrollbar;
}

struct scroll_ctrl *ScrollCtrlCreate(int32_t type, const struct event *event_src, const struct point *pos,
	int32_t obj_type, struct canvas *scrollbar)
{
	struct scroll_ctrl *self = MemAlloc(1, sizeof(struct scroll_ctrl));
	ScrollCtrlInit(self, type, event_src, pos, obj_type, scrollbar);

	return self;
}

void ScrollCtrlDestroy(struct scroll_ctrl **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void scroll_ctrl_destroy(struct object *obj)
{
	struct scroll_ctrl *self = (struct scroll_ctrl *) obj;

	if (self->scrollbar)
		ObjectDestroy(&self->scrollbar);

	obj->ops = &PICT_CTRL_OPS_TABLE.base.base.base.base.base;
	pict_ctrl_destroy(obj);
}

void scroll_ctrl_draw_children(struct view *view, struct screen_dc *dc, const struct rect *invalid_rect)
{
	struct scroll_ctrl *self = (struct scroll_ctrl *) view;

	button_draw_children(view, dc, invalid_rect);
	SHAPE_OPS(self->scrollbar)->DrawBrush20(&self->scrollbar->base, dc);
}
