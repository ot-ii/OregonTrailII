#pragma once

#include <stdint.h>

#include "modaldialog.h"

struct cstr;

struct click_dlg_ops {
	struct modal_dialog_ops base;
};

struct click_dlg {
	struct modal_dialog base;
};

#define CLICK_DLG_OPS(p) ((const struct click_dlg_ops *) ((const struct object *) (p))->ops)

extern const struct click_dlg_ops CLICK_DLG_OPS_TABLE;

void ClickDlgInit(struct click_dlg *self, struct cstr *title_src, int32_t img_id);
struct click_dlg *ClickDlgCreate(struct cstr *title_src, int32_t img_id);
void ClickDlgDestroy(struct click_dlg **ctx_mod);
void click_dlg_destroy(struct object *self);

void click_dlg_on_event(struct evt_handler *self, struct cmd_event *event);
