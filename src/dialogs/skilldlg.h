#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "modaldialog.h"

struct occup_mgr;
struct strrsrc;

struct skill_flags {
	bool checked[15];
	bool enabled[15];
};

struct skill_dlg_ops {
	struct modal_dialog_ops base;
};

struct skill_dlg {
	struct modal_dialog base;
	struct occup_mgr *body;
	struct skill_flags flags;
	int32_t points_left;
	struct strrsrc *owned;
};

#define SKILL_DLG_OPS(p) ((const struct skill_dlg_ops *) ((const struct object *) (p))->ops)

extern const struct skill_dlg_ops SKILL_DLG_OPS_TABLE;

void SkillDlgInit(struct skill_dlg *self, struct occup_mgr *body);
struct skill_dlg *SkillDlgCreate(struct occup_mgr *body);
void SkillDlgDestroy(struct skill_dlg **ctx_mod);
void skill_dlg_destroy(struct object *self);
void skill_dlg_begin(struct modal_dialog *self, intptr_t checks);
void skill_dlg_accept(struct modal_dialog *self, intptr_t out);
void skill_dlg_on_event(struct evt_handler *self, struct cmd_event *event);
void SkillDlgToggleOption(struct skill_dlg *self, int32_t id);
void SkillDlgUpdateTotal(struct skill_dlg *self);
