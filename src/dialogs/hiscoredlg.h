#pragma once

#include <stdint.h>

#include "modaldialog.h"

struct cstr;

struct hi_score_dlg_ops {
	struct modal_dialog_ops base;
};

struct hi_score_dlg {
	struct modal_dialog base;
};

#define HI_SCORE_DLG_OPS(p) ((const struct hi_score_dlg_ops *) ((const struct object *) (p))->ops)

extern const struct hi_score_dlg_ops HI_SCORE_DLG_OPS_TABLE;

void HiScoreDlgInit(struct hi_score_dlg *self);
struct hi_score_dlg *HiScoreDlgCreate(void);
void HiScoreDlgDestroy(struct hi_score_dlg **ctx_mod);
void hi_score_dlg_destroy(struct object *self);

void hi_score_dlg_on_event(struct evt_handler *self, struct cmd_event *event);
void hi_score_dlg_begin(struct modal_dialog *self, intptr_t param);
void hi_score_dlg_accept(struct modal_dialog *self, intptr_t param);

void HiscoreInsert(struct cstr *name, int32_t score);
