#pragma once

#include <stdint.h>

#include "modaldialog.h"

struct picker_dlg_ops {
	struct modal_dialog_ops base;
};

struct picker_dlg {
	struct modal_dialog base;
	int32_t wagons[3];
	int32_t people[3];
	int32_t sel;
};

#define PICKER_DLG_OPS(p) ((const struct picker_dlg_ops *) ((const struct object *) (p))->ops)

extern const struct picker_dlg_ops PICKER_DLG_OPS_TABLE;

void PickerDlgInit(struct picker_dlg *self);
struct picker_dlg *PickerDlgCreate(void);
void PickerDlgDestroy(struct picker_dlg **ctx_mod);
void picker_dlg_destroy(struct object *self);
void picker_dlg_accept(struct modal_dialog *self, intptr_t out);
void picker_dlg_begin(struct modal_dialog *self, intptr_t param);
void picker_dlg_on_event(struct evt_handler *self, struct cmd_event *event);
