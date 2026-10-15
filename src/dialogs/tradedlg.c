#include "tradedlg.h"

#include <stddef.h>

#include "../draw/canvas.h"
#include "../ui/cliplabelview.h"
#include "../ui/clipview.h"
#include "../ui/combobox.h"
#include "../ui/control.h"
#include "../base/cstr.h"
#include "../draw/drawable.h"
#include "../app/event.h"
#include "../game/game.h"
#include "../data/gfxdev.h"
#include "../game/invitem.h"
#include "../game/invlist.h"
#include "../game/journal.h"
#include "../base/list.h"
#include "../draw/mediabase.h"
#include "../base/memory.h"
#include "../trail/node.h"
#include "../app/otiiapp.h"
#include "../game/party.h"
#include "../ui/pictctrl.h"
#include "../draw/pictserver.h"
#include "../ui/picview.h"
#include "../base/rng.h"
#include "../screens/sceneassets.h"
#include "../draw/server.h"
#include "../draw/shape.h"
#include "../base/str.h"
#include "../data/strrsrc.h"
#include "../ui/textview.h"
#include "../ui/view.h"

static bool g_trade_nothing_wanted_shown;
static int32_t g_trade_offer = 0;
static int32_t g_trade_give_qty = 1;

const struct trade_dlg_ops TRADE_DLG_OPS_TABLE = {
	{
		{
			{trade_dlg_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			trade_dlg_on_event,
			evt_handler_on_idle,
			evt_handler_on_idle_spare,
		},
		trade_dlg_begin,
		trade_dlg_accept,
	},
};

void TradeDlgInitOffer(struct trade_dlg *self, struct cstr *offer_obj, int32_t offer, int32_t store_id,
	int32_t asset_id, int32_t sub_id, struct inv_list *party_items)
{
	ModalDialogInit(&self->base, 48, false, true);
	self->base.base.base.ops = &TRADE_DLG_OPS_TABLE.base.base.base;

	self->offer = offer;
	self->party_items = party_items;
	TradeDlgBuild(self, offer_obj, store_id, asset_id, sub_id);
}

void TradeDlgInit(struct trade_dlg *self, int32_t store_id, int32_t asset_id, int32_t sub_id,
	struct inv_list *party_items)
{
	ModalDialogInit(&self->base, 48, false, true);
	self->base.base.base.ops = &TRADE_DLG_OPS_TABLE.base.base.base;

	self->offer = 0;
	self->party_items = party_items;
	TradeDlgBuild(self, NULL, store_id, asset_id, sub_id);
}

struct trade_dlg *TradeDlgCreate(int32_t store_id, int32_t asset_id, int32_t sub_id, struct inv_list *party_items)
{
	struct trade_dlg *self = MemAlloc(1, sizeof(struct trade_dlg));
	TradeDlgInit(self, store_id, asset_id, sub_id, party_items);

	return self;
}

struct trade_dlg *TradeDlgCreateOffer(struct cstr *offer_obj, int32_t offer, int32_t store_id, int32_t asset_id,
	int32_t sub_id, struct inv_list *party_items)
{
	struct trade_dlg *self = MemAlloc(1, sizeof(struct trade_dlg));
	TradeDlgInitOffer(self, offer_obj, offer, store_id, asset_id, sub_id, party_items);

	return self;
}

void TradeDlgDestroy(struct trade_dlg **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

static void set_rect(struct rect *r, int32_t top, int32_t left, int32_t bottom, int32_t right)
{
	r->top = top;
	r->left = left;
	r->right = right;
	r->bottom = bottom;
}

static void offset_rect(struct rect *r, int32_t dh, int32_t dv)
{
	r->left += dh;
	r->right += dh;
	r->top += dv;
	r->bottom += dv;
}

static void add_child(struct trade_dlg *self, void *child)
{
	VIEW_OPS(self->base.content)->AddChild(self->base.content, (struct view *) child);
}

void TradeDlgBuild(struct trade_dlg *self, struct cstr *offer_obj, int32_t store_id, int32_t asset_id, int32_t sub_id)
{
	struct point pos;
	struct rect rect;
	struct rect src_rect;
	struct rect dst_rect;
	struct cstr *text;
	int32_t handlers[3];
	struct view *view;
	struct strrsrc *list;

	self->forced_zero = offer_obj ? true : false;

	if (self->forced_zero) {
		self->trader_items = NULL;

	} else {
		self->trader_items = InvListCreateStore(store_id + 1000, GetGameObj()->cur_node->price_zone, 20, -1);
		InvListPurgeEmpty(self->trader_items);
	}

	InvListPurgeEmpty(self->party_items);

	self->wanted_item_names = StrRsrcLoad(g_data_lib, store_id + 2500, true);
	self->base.title = CStrCreateFrom("Trading");

	pos.h = 0;
	pos.v = 0;
	self->base.content = (struct view *) PicViewCreatePoint(0, &pos, 180);

	struct drawable *backdrop = ServerCreateVirtualPacked((struct server *) g_pict_server, RegionalAssetId2000(),
		sub_id, true);
	struct canvas *image = OBJ_BASE_OPS(backdrop)->GetRect(&backdrop->base, &src_rect);
	set_rect(&dst_rect, 0, 0, src_rect.bottom - src_rect.top, src_rect.right - src_rect.left);
	struct canvas *canvas = CanvasCreateRect(&dst_rect, 8, 255);
	CANVAS_OPS(canvas)->StretchBlit(canvas, image, &src_rect, &dst_rect, 0, &dst_rect);
	ReleaseDrawable((struct media_base *) backdrop, false);
	offset_rect(&dst_rect, 7, 8);
	SHAPE_OPS(canvas)->SetRect((struct shape *) canvas, &dst_rect);
	struct clip_view *clip = ClipViewCreate(0, &dst_rect, (struct shape *) canvas, false);
	add_child(self, clip);

	struct drawable *portrait = ServerCreateVirtual((struct server *) g_pict_server, asset_id, true);

	if (!portrait)
		portrait = ServerCreateVirtual((struct server *) g_pict_server, 6001, true);

	offset_rect(&dst_rect, -7, -8);
	image = DRAWABLE_OPS(portrait)->GetRectAt(portrait, 0, &src_rect);
	struct point dst_pos = {dst_rect.top, dst_rect.left};
	CANVAS_OPS(canvas)->BlitImage(canvas, image, &src_rect, &dst_pos, &dst_rect, false);
	ReleaseDrawable((struct media_base *) portrait, false);

	if (self->forced_zero) {
		set_rect(&rect, 12, 220, 50, 445);
		text = offer_obj;

	} else {
		set_rect(&rect, 12, 220, 30, 445);
		text = CStrCreateFrom("Sure, I've got");
	}

	view = (struct view *) TextViewCreate(6, &rect, text, 3, 0, 0, 0, 0, 0, 0, 255, 0, 1);
	add_child(self, view);

	if (!self->forced_zero) {
		set_rect(&rect, 70, 220, 90, 475);
		text = CStrCreateFrom("that I'll trade to you for:");
		view = (struct view *) ClipLabelViewCreate(7, &rect, text, 3, 0, 1, 0, 0, 255, 0, 1);
		add_child(self, view);
		list = TradeDlgBuildInventoryList(self);
		self->give_item = InvItemFromNode(self->trader_items->base.head);
		set_rect(&rect, 40, 280, 60, 470);
		view = (struct view *) ComboBoxCreate(9002, &self->base.base.eh_event, &rect, 120, list, 0);
		((struct combo_box *) view)->font_id = 3;
		ViewNotifyChanged(view);
		add_child(self, view);
		list = TradeDlgBuildQuantityList(self);
		self->give_qty = 1;
		set_rect(&rect, 40, 220, 60, 275);
		view = (struct view *) ComboBoxCreate(9001, &self->base.base.eh_event, &rect, 120, list, 0);
		((struct combo_box *) view)->font_id = 3;
		ViewNotifyChanged(view);
		add_child(self, view);
	}

	self->matched = NULL;

	if (self->forced_zero)
		set_rect(&rect, 60, 240, 80, 445);
	else
		set_rect(&rect, 110, 240, 130, 445);

	view = (struct view *) ClipLabelViewCreate(5, &rect, CStrCreate(), 3, 0, 1, 0, 0, 255, 0, 1);
	add_child(self, view);
	set_rect(&rect, 170, 220, 190, 320);
	view = (struct view *) ClipLabelViewCreate(8, &rect, CStrCreateFrom("Is it a deal?"), 3, 0, 1, 0, 0, 255, 0, 1);
	add_child(self, view);
	set_rect(&rect, 170, 365, 190, 475);
	view = (struct view *) ClipLabelViewCreate(9, &rect, NULL, 3, 0, 1, 0, 0, 255, 0, 1);
	add_child(self, view);

	pos.h = 220;
	pos.v = 195;
	view = (struct view *) PictCtrlCreate(1, &self->base.base.eh_event, &pos, 2102, 0, 0, CStrCreateFrom("Yes"), 3, 1,
		1);
	add_child(self, view);
	pos.h = 310;
	pos.v = 195;
	view = (struct view *) PictCtrlCreate(3, &self->base.base.eh_event, &pos, 2102, 0, 0, CStrCreateFrom("Haggle"), 3,
		1, 1);
	add_child(self, view);
	pos.h = 400;
	pos.v = 195;
	view = (struct view *) PictCtrlCreate(2, &self->base.base.eh_event, &pos, 2102, 0, 0, CStrCreateFrom("No"), 3, 1,
		1);
	add_child(self, view);

	handlers[0] = CMD_CONTROL_ACTION;
	handlers[1] = CMD_EDIT_COMMITTED;
	handlers[2] = CMD_CLOSE_WINDOW;
	EvtHandlerBindHandler(&self->base.base, 3, handlers);
	g_trade_offer = 0;
}

void trade_dlg_destroy(struct object *obj)
{
	struct trade_dlg *self = (struct trade_dlg *) obj;

	if (self->trader_items)
		InvListDestroy(&self->trader_items);

	InvListDestroy(&self->party_items);
	StrRsrcDestroy(&self->wanted_item_names);

	obj->ops = &MODAL_DIALOG_OPS_TABLE.base.base;
	modal_dialog_destroy(obj);
}

void trade_dlg_accept(struct modal_dialog *dlg, intptr_t param)
{
	(void) param;
	struct trade_dlg *self = (struct trade_dlg *) dlg;
	struct party *party = GetGameObj()->party;

	if (!self->matched)
		return;

	struct otii_app *app = (struct otii_app *) g_app;
	JournalSetDate(&app->journal, GetGameObj()->date);

	if (!self->forced_zero) {
		struct inv_list *give_list = InvListCreate();
		InvListAdd(give_list, InvItemGetNameCStr(self->give_item), self->give_qty, 0.0);
		PartyDistributeLoot(party, give_list);
		InvListDestroy(&give_list);

		struct cstr *line = CStrCreateFrom("Traded ");
		struct cstr *part = CStrCreateInt(self->offered_qty, true);
		CStrAppendStr(line, part);
		CStrAppendBuf(line, " ");
		CStrDestroy(&part);

		if (self->offered_qty > 1)
			part = InvItemGetNamePlural(self->matched);
		else
			part = InvItemGetNameSingular(self->matched);

		CStrAppendStr(line, part);
		CStrDestroy(&part);
		CStrAppendBuf(line, " for ");
		part = CStrCreateInt(self->give_qty, true);
		CStrAppendStr(line, part);
		CStrAppendBuf(line, " ");
		CStrDestroy(&part);

		if (self->give_qty > 1)
			part = InvItemGetNamePlural(self->give_item);
		else
			part = InvItemGetNameSingular(self->give_item);

		CStrAppendStr(line, part);
		CStrDestroy(&part);
		CStrAppendBuf(line, ".");
		JournalAddLine(&app->journal, line);
	}

	if (CStrEqualBuf(InvItemGetNameCStr(self->matched), "cash")) {
		double cash = (double) self->offered_qty;
		party->cash = party->cash + -cash;

	} else {
		struct inv_item *item = InvListFindByName(&party->inventory, InvItemGetNameCStr(self->matched));

		if (item) {
			InvItemRemoveUnits(item, self->offered_qty);
			InvListPurgeEmpty(&party->inventory);

		} else {
			item = InvListFindByName(&party->animals, InvItemGetNameCStr(self->matched));

			if (item) {
				InvItemRemoveUnits(item, self->offered_qty);
				PartyPurgeEmptyAnimals(party);

			} else {
				item = InvListFindByName(&party->wagons, InvItemGetNameCStr(self->matched));

				if (item) {
					InvItemRemoveUnits(item, self->offered_qty);
					InvListPurgeEmpty(&party->wagons);

				} else {
					item = InvListFindByName(&party->spare_animals, InvItemGetNameCStr(self->matched));

					if (item) {
						InvItemRemoveUnits(item, self->offered_qty);
						InvListPurgeEmpty(&party->spare_animals);
					}
				}
			}
		}
	}
}

void trade_dlg_begin(struct modal_dialog *dlg, intptr_t param)
{
	(void) param;
	struct trade_dlg *self = (struct trade_dlg *) dlg;

	TradeDlgUpdateGiveQty(self);
	TradeDlgPickOffer(self);
	TradeDlgUpdateLabels(self);
}

void trade_dlg_on_event(struct evt_handler *handler, struct cmd_event *evt)
{
	struct trade_dlg *self = (struct trade_dlg *) handler;
	int32_t sel = evt->spec_b.id;

	if (evt->command == CMD_CONTROL_ACTION) {
		switch (sel) {
		case 1:
			self->base.result = DLG_OK;
			break;

		case 2:
			self->base.result = DLG_CANCEL;
			break;

		case 3:
			TradeDlgPickOffer(self);
			TradeDlgUpdateLabels(self);

			if (GetGameObj()->party->skills[SKILL_COMMERCE])
				g_trade_offer += 10;
			else
				g_trade_offer += 20;

			break;

		case 9001:
			TradeDlgUpdateGiveQty(self);
			TradeDlgPickOffer(self);
			TradeDlgUpdateLabels(self);
			break;

		case 9002:
			TradeDlgUpdateGiveItem(self);
			TradeDlgUpdateQtyCombo(self);
			TradeDlgUpdateGiveQty(self);
			TradeDlgPickOffer(self);
			TradeDlgUpdateLabels(self);
			g_trade_offer -= 50;

			if (g_trade_offer < 0)
				g_trade_offer = 0;

			break;
		}

		evt->handled = true;

	} else if (evt->command == CMD_EDIT_COMMITTED) {
		evt->handled = true;

	} else if (evt->command == CMD_CLOSE_WINDOW) {
		self->base.result = DLG_CANCEL;
		evt->handled = true;
	}
}

struct strrsrc *TradeDlgBuildQuantityList(struct trade_dlg *self)
{
	char buf[32];
	struct strrsrc *list = StrRsrcCreate();
	int32_t count = (int16_t) InvItemGetCount(self->give_item, false);
	g_trade_give_qty = 1;

	if (count > 150) {
		g_trade_give_qty = 25;
		count /= 25;

	} else if (count > 30) {
		g_trade_give_qty = 5;
		count /= 5;
	}

	for (int32_t index = 1; count >= index; ++index) {
		StrPrintf(buf, "%d", g_trade_give_qty * index);
		StrRsrcAppend(list, buf);
	}

	return list;
}

struct strrsrc *TradeDlgBuildInventoryList(struct trade_dlg *self)
{
	struct strrsrc *list = StrRsrcCreate();
	struct inv_item *node = InvItemFromNode(self->trader_items->base.head);

	while (node) {
		struct cstr *name = InvItemGetName(node);
		StrRsrcAppend(list, CStrGet(name));
		node = InvItemFromNode(node->node.next);
		CStrDestroy(&name);
	}

	return list;
}

void TradeDlgUpdateGiveQty(struct trade_dlg *self)
{
	if (self->forced_zero) {
		self->give_qty = 0;

	} else {
		struct control *ctrl = (struct control *) ViewFindById(self->base.content, 9001);
		self->give_qty = (int16_t) ((ctrl->value + 1) * g_trade_give_qty);
	}
}

void TradeDlgUpdateGiveItem(struct trade_dlg *self)
{
	if (self->forced_zero) {
		self->give_item = NULL;
		return;
	}

	struct control *ctrl = (struct control *) ViewFindById(self->base.content, 9002);
	int32_t count = ctrl->value;
	struct list_node *node = self->trader_items->base.head;

	while (count--)
		node = node->next;

	self->give_item = InvItemFromNode(node);
}

void TradeDlgPickOffer(struct trade_dlg *self)
{
	int32_t worth;
	int32_t roll;
	int32_t index;
	int32_t pick;
	int32_t target;
	double ratio = 0;
	int32_t count = StrRsrcCount(self->wanted_item_names);
	uint8_t *picked = MemAlloc(count > 0 ? (size_t) count : 1, 1);

	if (self->forced_zero)
		target = self->offer;
	else
		target = self->give_qty * InvItemGetUnitPrice(self->give_item);

	for (index = 0; count > index; ++index)
		picked[index] = 0;

	self->matched = NULL;
	self->offered_qty = 0;

	for (index = 0; count > index && !self->matched; ++index) {
		if (GetGameObj()->party->skills[SKILL_COMMERCE])
			roll = RandRangeAvg(60, 100, 1) + g_trade_offer;
		else
			roll = RandRangeAvg(75, 125, 1) + g_trade_offer;

		worth = roll * target / 100;

		do
			pick = RandRangeAvg(0, count - 1, 1);
		while (picked[pick]);

		picked[pick] = 1;
		struct cstr *name = StrRsrcGetCStr(self->wanted_item_names, pick);

		if (self->forced_zero || !CStrEqual(name, InvItemGetNameCStr(self->give_item))) {
			self->matched = InvListFindByName(self->party_items, name);

			if (self->matched) {
				self->offered_qty = (int16_t) (worth / InvItemGetUnitPrice(self->matched));

				if (self->offered_qty < 1)
					self->offered_qty = 1;

				if (InvItemGetCount(self->matched, true) < self->offered_qty)
					self->offered_qty = (int16_t) InvItemGetCount(self->matched, true);

				ratio = (double) worth / (InvItemGetUnitPrice(self->matched) * self->offered_qty);

				if (ratio > 1.5)
					self->matched = NULL;
			}
		}

		CStrDestroy(&name);
	}

	MemFree(picked);

	if (!self->matched)
		self->offered_qty = 0;
}

void TradeDlgUpdateLabels(struct trade_dlg *self)
{
	struct cstr *item_label;
	struct cstr *count_label;
	char buf[32];
	struct view *view;

	if (!self->matched) {
		if (self->forced_zero) {
			item_label = CStrCreateFrom("nothing you posess.");
			count_label = CStrCreate();

		} else {
			if (!g_trade_nothing_wanted_shown) {
				item_label = CStrCreateFrom("but you don't have anything I want for it.");
				view = ViewFindById(self->base.content, 7);
				ClipLabelViewSetLabelAttr((struct clip_label_view *) view, item_label);
				g_trade_nothing_wanted_shown = true;
			}

			item_label = CStrCreate();
			count_label = CStrCreate();
		}

	} else {
		if (!self->forced_zero && g_trade_nothing_wanted_shown) {
			item_label = CStrCreateFrom("that I'll trade to you for:");
			view = ViewFindById(self->base.content, 7);
			ClipLabelViewSetLabelAttr((struct clip_label_view *) view, item_label);
			g_trade_nothing_wanted_shown = false;
		}

		StrPrintf(buf, "%d ", self->offered_qty);
		item_label = CStrCreateFrom(buf);
		struct cstr *name;

		if (self->offered_qty == 1)
			name = InvItemGetNameSingular(self->matched);
		else
			name = InvItemGetNamePlural(self->matched);

		CStrAppendStr(item_label, name);
		CStrDestroy(&name);
		StrPrintf(buf, "(You have %d)", InvItemGetCount(self->matched, false));
		count_label = CStrCreateFrom(buf);
	}

	view = ViewFindById(self->base.content, 5);
	ClipLabelViewSetLabelAttr((struct clip_label_view *) view, item_label);
	view = ViewFindById(self->base.content, 9);
	ClipLabelViewSetLabelAttr((struct clip_label_view *) view, count_label);
}

static bool child_matches(const void *item, const void *key)
{
	const struct evt_handler *h = item;

	return EventMatches(key, &h->eh_event);
}

void TradeDlgUpdateQtyCombo(struct trade_dlg *self)
{
	struct event ev = EventMake(EVCLS_ANY, -1, -1);
	struct strrsrc *list = TradeDlgBuildQuantityList(self);
	int32_t count = (int16_t) InvItemGetCount(self->give_item, false);

	if (self->give_qty > count)
		self->give_qty = count;

	count = count / g_trade_give_qty;
	int32_t qty = self->give_qty / g_trade_give_qty;
	EventSetId(&ev, 9001);
	struct obarray *children = self->base.content->has_child_list ? self->base.content->child_list : NULL;
	struct combo_box *combo = ObarrayFind(children, child_matches, &ev);

	if (qty > count)
		qty = count;

	if (qty < 1)
		qty = 1;

	ComboBoxSetList(combo, list, qty - 1);
}
