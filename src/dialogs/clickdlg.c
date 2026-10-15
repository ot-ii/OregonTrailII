#include "clickdlg.h"

#include <stddef.h>

#include "../ui/button.h"
#include "../draw/drawable.h"
#include "../app/event.h"
#include "../base/memory.h"
#include "../draw/objbase.h"
#include "../ui/picview.h"
#include "../ui/view.h"

const struct click_dlg_ops CLICK_DLG_OPS_TABLE = {
	{
		{
			{click_dlg_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			click_dlg_on_event,
			evt_handler_on_idle,
			evt_handler_on_idle_spare,
		},
		modal_dialog_begin,
		modal_dialog_accept,
	},
};

void ClickDlgInit(struct click_dlg *self, struct cstr *title_src, int32_t img_id)
{
	ModalDialogInit(&self->base, 50, false, true);
	self->base.base.base.ops = &CLICK_DLG_OPS_TABLE.base.base.base;

	int32_t handled_cmds[2];
	struct point origin;
	struct rect rect;

	self->base.title = title_src;
	origin.h = 0;
	origin.v = 0;

	self->base.content = (struct view *) PicViewCreatePoint(0, &origin, img_id);

	struct drawable *image = ((struct pic_view *) self->base.content)->image;
	OBJ_BASE_OPS(image)->GetSize((struct obj_base *) image, &origin);

	rect.top = 0;
	rect.left = 0;
	rect.right = origin.h;
	rect.bottom = origin.v;

	struct view *button = (struct view *) ClipButtonCreate(1, &self->base.base.eh_event, &rect, CMD_CONTROL_ACTION,
		0, 0);
	VIEW_OPS(self->base.content)->AddChild(self->base.content, button);

	handled_cmds[0] = CMD_CONTROL_ACTION;
	handled_cmds[1] = CMD_CLOSE_WINDOW;
	EvtHandlerBindHandler(&self->base.base, 2, handled_cmds);
}

struct click_dlg *ClickDlgCreate(struct cstr *title_src, int32_t img_id)
{
	struct click_dlg *self = MemAlloc(1, sizeof(struct click_dlg));
	ClickDlgInit(self, title_src, img_id);

	return self;
}

void ClickDlgDestroy(struct click_dlg **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void click_dlg_destroy(struct object *obj)
{
	obj->ops = &MODAL_DIALOG_OPS_TABLE.base.base;
	modal_dialog_destroy(obj);
}

void click_dlg_on_event(struct evt_handler *self, struct cmd_event *event)
{
	((struct modal_dialog *) self)->result = DLG_OK;
	event->handled = true;
}
