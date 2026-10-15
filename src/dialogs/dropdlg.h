#pragma once

#include <stdint.h>

#include "modaldialog.h"

struct inv_list;

struct drop_dlg_ops {
	struct modal_dialog_ops base;
};

struct drop_dlg {
	struct modal_dialog base;
	struct inv_list *body;
	struct inv_list *merged;
};

#define DROP_DLG_OPS(p) ((const struct drop_dlg_ops *) ((const struct object *) (p))->ops)

extern const struct drop_dlg_ops DROP_DLG_OPS_TABLE;

void DropDlgInit(struct drop_dlg *self, struct inv_list *body, int32_t direct);
struct drop_dlg *DropDlgCreate(struct inv_list *body, int32_t direct);
void DropDlgDestroy(struct drop_dlg **ctx_mod);
void drop_dlg_destroy(struct object *self);

void drop_dlg_begin(struct modal_dialog *self, intptr_t param);
void drop_dlg_on_event(struct evt_handler *self, struct cmd_event *event);
void drop_dlg_accept(struct modal_dialog *self, intptr_t param);
