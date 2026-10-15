#include "uicontainer.h"

#include <stddef.h>

#include "../base/cstr.h"
#include "../draw/dispobj.h"
#include "../data/gfxdev.h"
#include "../base/memory.h"
#include "../draw/region.h"
#include "../data/strrsrc.h"
#include "../draw/text.h"
#include "../draw/textlabel.h"

const struct ui_container_ops UI_CONTAINER_OPS_TABLE = {
	{
		{
			{
				{
					{ui_container_destroy},
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

static void setup_region(struct ui_container *self, struct disp_obj *obj)
{
	struct button *button = &self->base;
	struct view *view = &button->base.base;

	button->data_src = obj;

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

void UiContainerInit(struct ui_container *self, int32_t id, const struct event *event, const struct rect *rect,
	int32_t type_id, int32_t ctrl_type, int32_t init_value, struct cstr *source, int32_t font_id, int32_t margin_x,
	int32_t margin_y)
{
	ButtonInitDispObj(&self->base, id, event, NULL, rect, ctrl_type, init_value);
	self->base.base.base.base.base.ops = &UI_CONTAINER_OPS_TABLE.base.base.base.base.base;

	struct rect content_rect = RectMake(0, 0, rect->bottom - rect->top, rect->right - rect->left);
	struct disp_obj *content = DispObjCreate(type_id, source, &content_rect, font_id, margin_x, margin_y);
	setup_region(self, content);
}

struct ui_container *UiContainerCreate(int32_t id, const struct event *event, const struct rect *rect,
	int32_t type_id, int32_t ctrl_type, int32_t init_value, struct cstr *source, int32_t font_id, int32_t margin_x,
	int32_t margin_y)
{
	struct ui_container *self = MemAlloc(1, sizeof(struct ui_container));
	UiContainerInit(self, id, event, rect, type_id, ctrl_type, init_value, source, font_id, margin_x, margin_y);

	return self;
}

void UiContainerDestroy(struct ui_container **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void ui_container_destroy(struct object *self)
{
	self->ops = &BUTTON_OPS_TABLE.base.base.base.base;
	button_destroy(self);
}

struct view *CreateUiContainer(struct ctrl_desc *desc)
{
	struct cstr *text = NULL;
	int32_t string_index = desc->item_index;
	struct rect rect = RectMake(desc->rect.top, desc->rect.left, desc->rect.bottom, desc->rect.right);

	if (string_index > -1) {
		if (desc->src) {
			text = StrRsrcGetCStr(desc->src, string_index);

		} else if (desc->rsrc_id > 0) {
			struct strrsrc *strings = StrRsrcLoad(g_text_lib, desc->rsrc_id, true);

			text = strings ? StrRsrcGetCStr(strings, string_index) : NULL;
			StrRsrcDestroy(&strings);
		}
	}

	struct ui_container *container = UiContainerCreate(desc->id, desc->event, &rect, desc->obj_type, desc->style,
		desc->value, text, desc->font_id, desc->margin_x, desc->margin_y);

	struct text_label *content = (struct text_label *) container->base.data_src;
	content->color_mode1 = (int16_t) desc->brush_color;

	int32_t color = (int16_t) desc->alt_brush_color;
	content->pen_color = color;

	if (content->text)
		content->text->base.base.penColor = color;

	content->color_default = (int16_t) desc->alt_brush_color;
	content->color_mode2 = (int16_t) desc->pen_color;

	if (desc->flags & 2)
		content->align = 1;

	ControlApplyDescriptor(&container->base.base, desc);

	return &container->base.base.base;
}
