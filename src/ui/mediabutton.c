#include "mediabutton.h"

#include <stddef.h>

#include "../app/evthandler.h"
#include "../base/memory.h"

const struct media_button_ops MEDIA_BUTTON_OPS_TABLE = {
	{
		{
			{
				{media_button_destroy},
				evt_handler_is_handled,
				evt_handler_set_handled_for,
				evt_handler_set_handled,
				evt_handler_clear_handled_for,
				evt_handler_clear_handled,
				media_button_on_event,
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
	},
};

void MediaButtonInit(struct media_button *self, int32_t id, const struct point *pos, int32_t source_id,
	int32_t bg_id, const struct point *anchor, int32_t fg_id, const struct event *spec_src, int32_t cmd,
	int32_t notify_p1, int32_t notify_p2)
{
	MediaViewInit(&self->base, id, pos, source_id, bg_id, anchor, fg_id);
	self->base.base.base.base.ops = &MEDIA_BUTTON_OPS_TABLE.base.base.base.base;

	self->spec = EventMake(EVCLS_ANY, -1, -1);
	MediaButtonStart(self, spec_src, cmd, notify_p1, notify_p2);
}

struct media_button *MediaButtonCreate(int32_t id, const struct point *pos, int32_t source_id, int32_t bg_id,
	const struct point *anchor, int32_t fg_id, const struct event *spec_src, int32_t cmd, int32_t notify_p1,
	int32_t notify_p2)
{
	struct media_button *self = MemAlloc(1, sizeof(struct media_button));
	MediaButtonInit(self, id, pos, source_id, bg_id, anchor, fg_id, spec_src, cmd, notify_p1, notify_p2);

	return self;
}

void MediaButtonDestroy(struct media_button **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void media_button_destroy(struct object *self)
{
	self->ops = &MEDIA_VIEW_OPS_TABLE.base.base.base;
	media_view_destroy(self);
}

struct view *MediaButtonFromTemplate(struct ctrl_desc *desc)
{
	struct point origin = PointMake(desc->rect.left, desc->rect.top);
	struct point margin = PointMake(desc->margin_x, desc->margin_y);

	struct media_button *button = MediaButtonCreate(desc->id, &origin, desc->obj_type, desc->style, &margin,
		desc->arg7, desc->event, CMD_CONTROL_ACTION, 0, 0);

	if (button->base.source) {
		if (desc->cmd_id) {
			button->notify_cmd = desc->cmd_id;
			button->notify_p1 = desc->cmd_param1;
			button->notify_p2 = desc->cmd_param2;
		}

		if (desc->cursor_id > 0)
			VIEW_OPS(button)->SetCursorId(&button->base.base, desc->cursor_id);

	} else {
		MediaButtonDestroy(&button);
	}

	return (struct view *) button;
}

void media_button_on_event(struct evt_handler *self, struct cmd_event *event)
{
	struct media_button *b = (struct media_button *) self;

	if (event->command == CMD_MOUSE) {
		if (((struct mouse_event *) event)->kind == MOUSE_DOWN) {
			event->handled = true;

			struct cmd_event *cmd = CmdEventCreateSpecsFlag(b->notify_cmd, &self->eh_event, &b->spec, false);
			cmd->param1 = b->notify_p1;
			cmd->param2 = b->notify_p2;
			DispatchGlobalEvent(cmd);
		}
	}
}

void MediaButtonStart(struct media_button *self, const struct event *spec_src, int32_t cmd, int32_t notify_p1,
	int32_t notify_p2)
{
	self->spec.code = spec_src->code;
	self->spec.id = spec_src->id;
	self->spec.event_code = spec_src->event_code;
	self->notify_cmd = cmd;
	self->notify_p1 = notify_p1;
	self->notify_p2 = notify_p2;

	int32_t match_spec = CMD_MOUSE;
	EvtHandlerBindHandler(&self->base.base.base, 1, &match_spec);
}
