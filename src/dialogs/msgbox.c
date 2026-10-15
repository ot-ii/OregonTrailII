#include "msgbox.h"

#include <stddef.h>

#include "../base/cstr.h"
#include "../app/event.h"
#include "../ui/framewin.h"
#include "../game/invitem.h"
#include "../game/invlist.h"
#include "../base/memory.h"
#include "../ui/pictctrl.h"
#include "../ui/picview.h"
#include "../ui/textview.h"
#include "../ui/view.h"

static const char g_msgbox_title[] = "Oregon Trail II";

const struct msg_box_ops MSG_BOX_OPS_TABLE = {
	{
		{
			{msg_box_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			msg_box_on_event,
			evt_handler_on_idle,
			evt_handler_on_idle_spare,
		},
		modal_dialog_begin,
		modal_dialog_accept,
	},
};

void MsgBoxInit(struct msg_box *self, struct cstr *text, bool two_buttons)
{
	ModalDialogInit(&self->base, 47, false, true);
	self->base.base.base.ops = &MSG_BOX_OPS_TABLE.base.base.base;

	struct point pos;
	struct rect rect;
	int32_t handlers[2];
	struct view *child;
	struct view *content;

	self->base.title = CStrCreateFrom(g_msgbox_title);

	pos.h = 0;
	pos.v = 0;
	self->base.content = (struct view *) PicViewCreatePoint(0, &pos, 2600);
	content = self->base.content;

	pos.h = 20;
	pos.v = 20;
	child = (struct view *) PicViewCreatePoint(0, &pos, 3);
	VIEW_OPS(content)->AddChild(content, child);

	rect.top = 20;
	rect.left = 65;
	rect.right = 370;
	rect.bottom = 160;
	child = (struct view *) TextViewCreate(5, &rect, text, 3, 0, 0, 0, 0, 0, 0, 255, 0, 1);
	VIEW_OPS(content)->AddChild(content, child);

	if (two_buttons) {
		pos.h = 80;
		pos.v = 170;
		child = (struct view *) PictCtrlCreate(2, &self->base.base.eh_event, &pos, 2102, 0, 0,
			CStrCreateFrom("Cancel"), 3, 1, 1);
		VIEW_OPS(content)->AddChild(content, child);
		pos.h = 240;
		pos.v = 170;

	} else {
		pos.h = 160;
		pos.v = 170;
	}

	child = (struct view *) PictCtrlCreate(1, &self->base.base.eh_event, &pos, 2102, 0, 0, CStrCreateFrom("OK"), 3,
		1, 1);
	VIEW_OPS(content)->AddChild(content, child);

	handlers[0] = CMD_CONTROL_ACTION;
	handlers[1] = CMD_CLOSE_WINDOW;
	EvtHandlerBindHandler(&self->base.base, 2, handlers);
}

struct msg_box *MsgBoxCreate(struct cstr *text, bool two_buttons)
{
	struct msg_box *self = MemAlloc(1, sizeof(struct msg_box));
	MsgBoxInit(self, text, two_buttons);

	return self;
}

void MsgBoxDestroy(struct msg_box **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void msg_box_destroy(struct object *obj)
{
	obj->ops = &MODAL_DIALOG_OPS_TABLE.base.base;
	modal_dialog_destroy(obj);
}

void msg_box_on_event(struct evt_handler *self, struct cmd_event *evt)
{
	struct modal_dialog *dlg = (struct modal_dialog *) self;

	if (evt->command == CMD_CONTROL_ACTION) {
		if (evt->spec_b.id == 1)
			dlg->result = DLG_OK;
		else
			dlg->result = DLG_CANCEL;

	} else {
		if (evt->command == CMD_CLOSE_WINDOW)
			dlg->result = DLG_CANCEL;
	}

	evt->handled = true;
}

void ShowItemListBox(const char *lead, struct inv_list *list)
{
	struct inv_item *cursor = InvItemFromNode(list->base.head);
	struct cstr *items = InvListFormatContentsPage(list, &cursor, true);
	struct cstr *text = CStrCreateFrom(lead);

	CStrAppendStr(text, items);
	CStrDestroy(&items);

	struct msg_box *box = MsgBoxCreate(text, false);
	ModalDialogRun(&box->base, 0, (struct window *) g_parent_win);
	MsgBoxDestroy(&box);
}
