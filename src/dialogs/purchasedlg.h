#pragma once

#include <stdint.h>

#include "modaldialog.h"

struct inv_list;

struct purchase_dlg_ops {
	struct modal_dialog_ops base;
};

struct purchase_dlg {
	struct modal_dialog base;
	struct inv_list *body;
};

#define PURCHASE_DLG_OPS(p) ((const struct purchase_dlg_ops *) ((const struct object *) (p))->ops)

extern const struct purchase_dlg_ops PURCHASE_DLG_OPS_TABLE;

void PurchaseDlgInit(struct purchase_dlg *self, struct inv_list *body, int32_t direct, double capacity);
struct purchase_dlg *PurchaseDlgCreate(struct inv_list *body, int32_t direct, double capacity);
void PurchaseDlgDestroy(struct purchase_dlg **ctx_mod);
void purchase_dlg_destroy(struct object *self);
void purchase_dlg_begin(struct modal_dialog *self, intptr_t param);
void purchase_dlg_accept(struct modal_dialog *self, intptr_t party);
void purchase_dlg_on_event(struct evt_handler *self, struct cmd_event *event);
