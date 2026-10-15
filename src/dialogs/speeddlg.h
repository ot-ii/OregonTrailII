#pragma once

#include <stdint.h>

#include "modaldialog.h"

struct speed_dlg_ops {
	struct modal_dialog_ops base;
};

struct speed_dlg {
	struct modal_dialog base;
};

#define SPEED_DLG_OPS(p) ((const struct speed_dlg_ops *) ((const struct object *) (p))->ops)

extern const struct speed_dlg_ops SPEED_DLG_OPS_TABLE;

void SpeedDlgInit(struct speed_dlg *self);
struct speed_dlg *SpeedDlgCreate(void);
void SpeedDlgDestroy(struct speed_dlg **ctx_mod);
void speed_dlg_destroy(struct object *self);
void speed_dlg_accept(struct modal_dialog *self, intptr_t s);
void speed_dlg_begin(struct modal_dialog *self, intptr_t s);
void speed_dlg_on_event(struct evt_handler *self, struct cmd_event *event);
