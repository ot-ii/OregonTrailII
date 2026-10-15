#include "purchasedlg.h"

#include <stddef.h>

#include "../base/cstr.h"
#include "../app/event.h"
#include "../game/game.h"
#include "../screens/invgrid.h"
#include "../game/invitem.h"
#include "../game/invlist.h"
#include "../game/journal.h"
#include "../base/memory.h"
#include "../app/otiiapp.h"
#include "../game/party.h"
#include "../ui/pictctrl.h"
#include "../ui/view.h"

const struct purchase_dlg_ops PURCHASE_DLG_OPS_TABLE = {
	{
		{
			{purchase_dlg_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			purchase_dlg_on_event,
			evt_handler_on_idle,
			evt_handler_on_idle_spare,
		},
		purchase_dlg_begin,
		purchase_dlg_accept,
	},
};

void PurchaseDlgInit(struct purchase_dlg *self, struct inv_list *body, int32_t direct, double capacity)
{
	ModalDialogInit(&self->base, 1, false, true);
	self->base.base.base.ops = &PURCHASE_DLG_OPS_TABLE.base.base.base;

	struct point pos;
	struct view *child;
	struct cstr *label_text;
	int32_t handler_ids[2];
	const struct event *ev = &self->base.base.eh_event;

	self->body = body;
	self->base.title = CStrCreateFrom("Purchase Inventory");

	pos.h = 0;
	pos.v = 0;
	self->base.content = (struct view *) StoreGridCreate(0, &pos, 2100, body, direct, capacity);
	struct view *content = self->base.content;

	pos.h = 120;
	pos.v = 315;
	label_text = CStrCreateFrom("Purchase");
	child = (struct view *) PictCtrlCreate(1, ev, &pos, 2102, 0, 0, label_text, 3, 0, 0);
	VIEW_OPS(content)->AddChild(content, child);

	pos.h = 230;
	pos.v = 315;
	label_text = CStrCreateFrom("Cancel");
	child = (struct view *) PictCtrlCreate(2, ev, &pos, 2102, 0, 0, label_text, 3, 0, 0);
	VIEW_OPS(content)->AddChild(content, child);

	handler_ids[0] = CMD_CONTROL_ACTION;
	handler_ids[1] = CMD_CLOSE_WINDOW;
	EvtHandlerBindHandler(&self->base.base, 2, handler_ids);
}

struct purchase_dlg *PurchaseDlgCreate(struct inv_list *body, int32_t direct, double capacity)
{
	struct purchase_dlg *self = MemAlloc(1, sizeof(struct purchase_dlg));
	PurchaseDlgInit(self, body, direct, capacity);

	return self;
}

void PurchaseDlgDestroy(struct purchase_dlg **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void purchase_dlg_destroy(struct object *obj)
{
	obj->ops = &MODAL_DIALOG_OPS_TABLE.base.base;
	modal_dialog_destroy(obj);
}

void purchase_dlg_begin(struct modal_dialog *self, intptr_t param)
{
	(void) param;
	InvGridRefresh((struct inv_grid *) self->content);
}

void purchase_dlg_accept(struct modal_dialog *self, intptr_t party)
{
	struct purchase_dlg *dlg = (struct purchase_dlg *) self;

	if (self->result == DLG_CANCEL)
		return;

	struct journal *journal = &((struct otii_app *) g_app)->journal;
	int32_t item_count = 0;
	struct party *party_obj = (struct party *) party;
	struct inv_list *bought_list = InvListCreate();
	struct inv_item *item = InvItemFromNode(dlg->body->base.head);
	int32_t total_cents = 0;

	JournalSetDate(journal, GetGameObj()->date);
	struct cstr *entry_text = CStrCreateFrom("Purchased ");

	while (item) {
		int32_t qty = item->selected_qty;

		if (qty) {
			InvListAdd(bought_list, InvItemGetNameCStr(item), qty, 0.0);
			total_cents += qty * InvItemGetUnitPrice(item);
			InvItemRemoveUnits(item, qty);

			if (item_count)
				CStrAppendBuf(entry_text, ", ");

			struct cstr *piece_text = CStrCreateInt(qty, true);
			CStrAppendStr(entry_text, piece_text);
			CStrAppendBuf(entry_text, " ");
			CStrDestroy(&piece_text);

			if (qty > 1)
				piece_text = InvItemGetNamePlural(item);
			else
				piece_text = InvItemGetNameSingular(item);

			CStrAppendStr(entry_text, piece_text);
			CStrDestroy(&piece_text);
			++item_count;
		}

		item = InvItemFromNode(item->node.next);
	}

	if (item_count) {
		CStrAppendBuf(entry_text, ".");
		JournalAddLine(journal, entry_text);

	} else {
		CStrDestroy(&entry_text);
	}

	InvListPurgeEmpty(dlg->body);
	PartyDistributeLoot(party_obj, bought_list);
	InvListDestroy(&bought_list);
	party_obj->cash = (double) -total_cents / 100.0 + party_obj->cash;
}

void purchase_dlg_on_event(struct evt_handler *self, struct cmd_event *evt)
{
	struct modal_dialog *dlg = (struct modal_dialog *) self;
	int32_t sel = evt->spec_b.id;

	if (evt->command == CMD_CONTROL_ACTION) {
		switch (sel) {
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
