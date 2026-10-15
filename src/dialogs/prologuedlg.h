#pragma once

#include <stdint.h>

#include "modaldialog.h"

struct prologue_dlg_ops {
	struct modal_dialog_ops base;
};

struct prologue_dlg {
	struct modal_dialog base;
};

#define PROLOGUE_DLG_OPS(p) ((const struct prologue_dlg_ops *) ((const struct object *) (p))->ops)

extern const struct prologue_dlg_ops PROLOGUE_DLG_OPS_TABLE;

void PrologueDlgInit(struct prologue_dlg *self);
struct prologue_dlg *PrologueDlgCreate(void);
void PrologueDlgDestroy(struct prologue_dlg **ctx_mod);
void prologue_dlg_destroy(struct object *self);

void prologue_dlg_on_event(struct evt_handler *self, struct cmd_event *event);
void prologue_dlg_begin(struct modal_dialog *self, intptr_t param);
void prologue_dlg_accept(struct modal_dialog *self, intptr_t param);
