#include "piccontrol.h"

#include <stddef.h>

#include "../draw/drawable.h"
#include "../base/memory.h"

const struct pic_control_ops PIC_CONTROL_OPS_TABLE = {
	{
		{
			{
				{pic_control_destroy},
				evt_handler_is_handled,
				evt_handler_set_handled_for,
				evt_handler_set_handled,
				evt_handler_clear_handled_for,
				evt_handler_clear_handled,
				pic_control_on_event,
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
			pic_view_draw_children,
			view_move_children,
		},
	},
};

static void set_ops(struct pic_control *self)
{
	self->base.base.base.base.ops = &PIC_CONTROL_OPS_TABLE.base.base.base.base;
}

void PicControlInitImage(struct pic_control *self, int32_t id, const struct rect *rect, struct drawable *image,
	const struct event *cmd, int32_t cmd_code, int32_t cmd_param1, int32_t cmd_param2)
{
	PicViewInitImage(&self->base, id, rect, image);
	set_ops(self);
	self->pic_event = EventMake(EVCLS_ANY, -1, -1);

	PicControlSetCommand(self, cmd, cmd_code, cmd_param1, cmd_param2);
}

void PicControlInit(struct pic_control *self, int32_t id, const struct rect *rect, int32_t img_id,
	const struct event *cmd, int32_t cmd_code, int32_t cmd_param1, int32_t cmd_param2)
{
	PicViewInit(&self->base, id, rect, img_id);
	set_ops(self);
	self->pic_event = EventMake(EVCLS_ANY, -1, -1);

	PicControlSetCommand(self, cmd, cmd_code, cmd_param1, cmd_param2);
}

void PicControlInitPointEmpty(struct pic_control *self, int32_t id, const struct point *pos,
	const struct event *cmd, int32_t cmd_code, int32_t cmd_param1, int32_t cmd_param2)
{
	PicViewInitPointEmpty(&self->base, id, pos);
	set_ops(self);
	self->pic_event = EventMake(EVCLS_ANY, -1, -1);

	PicControlSetCommand(self, cmd, cmd_code, cmd_param1, cmd_param2);
}

void PicControlInitPoint(struct pic_control *self, int32_t id, const struct point *pos, int32_t img_id,
	const struct event *cmd, int32_t cmd_code, int32_t cmd_param1, int32_t cmd_param2)
{
	PicViewInitPoint(&self->base, id, pos, img_id);
	set_ops(self);
	self->pic_event = EventMake(EVCLS_ANY, -1, -1);

	PicControlSetCommand(self, cmd, cmd_code, cmd_param1, cmd_param2);
}

struct pic_control *PicControlCreateImage(int32_t id, const struct rect *rect, struct drawable *image,
	const struct event *cmd, int32_t cmd_code, int32_t cmd_param1, int32_t cmd_param2)
{
	struct pic_control *self = MemAlloc(1, sizeof(struct pic_control));
	PicControlInitImage(self, id, rect, image, cmd, cmd_code, cmd_param1, cmd_param2);

	return self;
}

struct pic_control *PicControlCreate(int32_t id, const struct rect *rect, int32_t img_id, const struct event *cmd,
	int32_t cmd_code, int32_t cmd_param1, int32_t cmd_param2)
{
	struct pic_control *self = MemAlloc(1, sizeof(struct pic_control));
	PicControlInit(self, id, rect, img_id, cmd, cmd_code, cmd_param1, cmd_param2);

	return self;
}

struct pic_control *PicControlCreatePoint(int32_t id, const struct point *pos, int32_t img_id,
	const struct event *cmd, int32_t cmd_code, int32_t cmd_param1, int32_t cmd_param2)
{
	struct pic_control *self = MemAlloc(1, sizeof(struct pic_control));
	PicControlInitPoint(self, id, pos, img_id, cmd, cmd_code, cmd_param1, cmd_param2);

	return self;
}

struct pic_control *PicControlCreatePointEmpty(int32_t id, const struct point *pos, const struct event *cmd,
	int32_t cmd_code, int32_t cmd_param1, int32_t cmd_param2)
{
	struct pic_control *self = MemAlloc(1, sizeof(struct pic_control));
	PicControlInitPointEmpty(self, id, pos, cmd, cmd_code, cmd_param1, cmd_param2);

	return self;
}

void PicControlDestroy(struct pic_control **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void pic_control_destroy(struct object *self)
{
	self->ops = &PIC_VIEW_OPS_TABLE.base.base.base;
	pic_view_destroy(self);
}

struct view *PicControlCreateFromDesc(struct ctrl_desc *desc)
{
	struct pic_control *result;
	struct rect rect = RectMake(desc->rect.top, desc->rect.left, desc->rect.bottom, desc->rect.right);

	if (rect.bottom != -1) {
		result = PicControlCreate(desc->id, &rect, desc->obj_type, desc->event, CMD_CONTROL_ACTION, 0, 0);

	} else {
		struct point top_left = RectTopLeft(&rect);
		result = PicControlCreatePoint(desc->id, &top_left, desc->obj_type, desc->event, CMD_CONTROL_ACTION, 0, 0);
	}

	if (result->base.image) {
		if (desc->cmd_id != 0) {
			result->cmd_code = desc->cmd_id;
			result->cmd_param1 = desc->cmd_param1;
			result->cmd_param2 = desc->cmd_param2;
		}

		if (desc->cursor_id > 0)
			VIEW_OPS(result)->SetCursorId(&result->base.base, desc->cursor_id);

	} else {
		ObjectDelete(result);
		return NULL;
	}

	return &result->base.base;
}

void pic_control_on_event(struct evt_handler *handler, struct cmd_event *evt)
{
	struct pic_control *self = (struct pic_control *) handler;

	if (evt->command == CMD_MOUSE) {
		if (((struct mouse_event *) evt)->kind == MOUSE_DOWN) {
			evt->handled = true;

			struct cmd_event *cmd = CmdEventCreateSpecsFlag(self->cmd_code, &handler->eh_event, &self->pic_event,
				false);
			cmd->param1 = self->cmd_param1;
			cmd->param2 = self->cmd_param2;
			DispatchGlobalEvent(cmd);
		}
	}
}

void PicControlSetCommand(struct pic_control *self, const struct event *cmd, int32_t cmd_code, int32_t cmd_param1,
	int32_t cmd_param2)
{
	self->pic_event.code = cmd->code;
	self->pic_event.id = cmd->id;
	self->pic_event.event_code = cmd->event_code;
	self->cmd_code = cmd_code;
	self->cmd_param1 = cmd_param1;
	self->cmd_param2 = cmd_param2;

	int32_t evt_id = CMD_MOUSE;
	EvtHandlerBindHandler(&self->base.base.base, 1, &evt_id);
}
