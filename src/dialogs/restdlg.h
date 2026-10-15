#pragma once

#include <stdint.h>

#include "modaldialog.h"

struct rest_dlg_ops {
	struct modal_dialog_ops base;
};

struct rest_dlg {
	struct modal_dialog base;
};

#define REST_DLG_OPS(p) ((const struct rest_dlg_ops *) ((const struct object *) (p))->ops)

extern const struct rest_dlg_ops REST_DLG_OPS_TABLE;

void RestDlgInit(struct rest_dlg *self);
struct rest_dlg *RestDlgCreate(void);
void RestDlgDestroy(struct rest_dlg **ctx_mod);
void rest_dlg_destroy(struct object *self);
void rest_dlg_begin(struct modal_dialog *self, intptr_t out);
void rest_dlg_accept(struct modal_dialog *self, intptr_t out);
void rest_dlg_on_event(struct evt_handler *self, struct cmd_event *event);
