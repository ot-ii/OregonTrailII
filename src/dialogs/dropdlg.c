#include "dropdlg.h"

#include <stddef.h>

#include "../base/cstr.h"
#include "../app/event.h"
#include "../game/game.h"
#include "../screens/invgrid.h"
#include "../game/invitem.h"
#include "../game/invlist.h"
#include "../base/list.h"
#include "../base/memory.h"
#include "../game/party.h"
#include "../ui/pictctrl.h"
#include "../ui/view.h"

const struct drop_dlg_ops DROP_DLG_OPS_TABLE = {
	{
		{
			{drop_dlg_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			drop_dlg_on_event,
			evt_handler_on_idle,
			evt_handler_on_idle_spare,
		},
		drop_dlg_begin,
		drop_dlg_accept,
	},
};

void DropDlgInit(struct drop_dlg *self, struct inv_list *body, int32_t direct)
{
	ModalDialogInit(&self->base, 1, false, true);
	self->base.base.base.ops = &DROP_DLG_OPS_TABLE.base.base.base;

	struct cstr *label;
	int32_t handled_cmds[2];
	struct view *child;
	struct point origin;
	const struct event *ev = &self->base.base.eh_event;

	self->body = body;
	self->merged = InvListCreate();
	struct inv_item *item = InvItemFromNode(self->body->base.head);

	while (item) {
		struct inv_item *existing = InvListFindByName(self->merged, InvItemGetNameCStr(item));

		if (existing)
			InvItemAddUnits(existing, InvItemGetCount(item, false));
		else
			InvListAdd(self->merged, InvItemGetNameCStr(item), InvItemGetCount(item, false), 0.0);

		item = InvItemFromNode(item->node.next);
	}

	self->base.title = CStrCreateFrom("Dump Inventory");

	origin.h = 0;
	origin.v = 0;
	self->base.content = (struct view *) DropGridCreate(-2, &origin, 2100, self->merged, direct);
	struct view *content = self->base.content;

	origin.h = 120;
	origin.v = 315;
	label = CStrCreateFrom("Dump");
	child = (struct view *) PictCtrlCreate(1, ev, &origin, 2102, 0, 0, label, 0, 0, 0);
	VIEW_OPS(content)->AddChild(content, child);

	origin.h = 230;
	origin.v = 315;
	label = CStrCreateFrom("Cancel");
	child = (struct view *) PictCtrlCreate(2, ev, &origin, 2102, 0, 0, label, 0, 0, 0);
	VIEW_OPS(content)->AddChild(content, child);

	handled_cmds[0] = CMD_CONTROL_ACTION;
	handled_cmds[1] = CMD_CLOSE_WINDOW;
	EvtHandlerBindHandler(&self->base.base, 2, handled_cmds);
}

struct drop_dlg *DropDlgCreate(struct inv_list *body, int32_t direct)
{
	struct drop_dlg *self = MemAlloc(1, sizeof(struct drop_dlg));
	DropDlgInit(self, body, direct);

	return self;
}

void DropDlgDestroy(struct drop_dlg **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void drop_dlg_destroy(struct object *obj)
{
	struct drop_dlg *self = (struct drop_dlg *) obj;

	InvListDestroy(&self->merged);

	obj->ops = &MODAL_DIALOG_OPS_TABLE.base.base;
	modal_dialog_destroy(obj);
}

void drop_dlg_begin(struct modal_dialog *self, intptr_t param)
{
	(void) param;
	InvGridRefresh((struct inv_grid *) self->content);
}

void drop_dlg_accept(struct modal_dialog *self, intptr_t param)
{
	(void) param;
	struct drop_dlg *dlg = (struct drop_dlg *) self;
	struct inv_item *next_item;

	if (self->result == DLG_CANCEL)
		return;

	for (struct inv_item *item = InvItemFromNode(dlg->merged->base.head); item; item = next_item) {
		next_item = InvItemFromNode(item->node.next);
		int32_t remaining = item->selected_qty;

		while (remaining > 0) {
			struct inv_item *stock_item = InvListFindByName(dlg->body, InvItemGetNameCStr(item));
			int32_t have = InvItemGetCount(stock_item, false);

			if (!InvItemRemoveUnits(stock_item, remaining)) {
				ListRemove(&dlg->body->base, &stock_item->node);
				ObjectDelete(stock_item);
			}

			remaining -= have;
		}
	}

	if (GetGameObj())
		PartyCalcPullingMode(GetGameObj()->party);
}

void drop_dlg_on_event(struct evt_handler *self, struct cmd_event *evt)
{
	struct modal_dialog *dlg = (struct modal_dialog *) self;
	int32_t control_id = evt->spec_b.id;

	if (evt->command == CMD_CONTROL_ACTION) {
		switch (control_id) {
			case 1:
				dlg->result = DLG_OK;
				break;
			case 2:
				dlg->result = DLG_CANCEL;
				break;
		}

		evt->handled = true;

	} else {
		if (evt->command == CMD_CLOSE_WINDOW) {
			dlg->result = DLG_CANCEL;
			evt->handled = true;
		}
	}
}
