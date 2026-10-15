#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "modaldialog.h"

struct cstr;
struct inv_item;
struct inv_list;
struct strrsrc;

struct trade_dlg_ops {
	struct modal_dialog_ops base;
};

struct trade_dlg {
	struct modal_dialog base;
	struct inv_list *trader_items;
	struct inv_list *party_items;
	int32_t give_qty;
	struct inv_item *give_item;
	int32_t offered_qty;
	struct inv_item *matched;
	struct strrsrc *wanted_item_names;
	bool forced_zero;
	int32_t offer;
};

#define TRADE_DLG_OPS(p) ((const struct trade_dlg_ops *) ((const struct object *) (p))->ops)

extern const struct trade_dlg_ops TRADE_DLG_OPS_TABLE;

void TradeDlgInit(struct trade_dlg *self, int32_t store_id, int32_t asset_id, int32_t sub_id,
	struct inv_list *party_items);
void TradeDlgInitOffer(struct trade_dlg *self, struct cstr *offer_obj, int32_t offer, int32_t store_id,
	int32_t asset_id, int32_t sub_id, struct inv_list *party_items);
struct trade_dlg *TradeDlgCreate(int32_t store_id, int32_t asset_id, int32_t sub_id, struct inv_list *party_items);
struct trade_dlg *TradeDlgCreateOffer(struct cstr *offer_obj, int32_t offer, int32_t store_id, int32_t asset_id,
	int32_t sub_id, struct inv_list *party_items);
void TradeDlgDestroy(struct trade_dlg **ctx_mod);
void trade_dlg_destroy(struct object *self);

void TradeDlgBuild(struct trade_dlg *self, struct cstr *offer_obj, int32_t store_id, int32_t asset_id, int32_t sub_id);

struct strrsrc *TradeDlgBuildInventoryList(struct trade_dlg *self);
struct strrsrc *TradeDlgBuildQuantityList(struct trade_dlg *self);
void TradeDlgUpdateGiveQty(struct trade_dlg *self);
void TradeDlgUpdateGiveItem(struct trade_dlg *self);
void trade_dlg_begin(struct modal_dialog *self, intptr_t param);
void TradeDlgPickOffer(struct trade_dlg *self);
void TradeDlgUpdateLabels(struct trade_dlg *self);
void TradeDlgUpdateQtyCombo(struct trade_dlg *self);
void trade_dlg_on_event(struct evt_handler *self, struct cmd_event *event);
void trade_dlg_accept(struct modal_dialog *self, intptr_t param);
