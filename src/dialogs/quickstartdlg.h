#pragma once

#include <stdint.h>

#include "modaldialog.h"

struct party;

enum quick_start_cmd {
	CMD_QUICK_START_CANCEL = 7001,
	CMD_QUICK_START_ACCEPT = 7002,
	CMD_QUICK_START_REROLL = 7003,
};

struct quick_start_choices {
	struct party *party;
	int32_t month;
	int32_t year;
	int32_t start_node;
	int32_t destination;
	int32_t wagons;
	int32_t people;
	int32_t oxen;
};

struct quick_start_dlg_ops {
	struct modal_dialog_ops base;
};

struct quick_start_dlg {
	struct modal_dialog base;
	struct quick_start_choices *choices;
};

#define QUICK_START_DLG_OPS(p) ((const struct quick_start_dlg_ops *) ((const struct object *) (p))->ops)

extern const struct quick_start_dlg_ops QUICK_START_DLG_OPS_TABLE;

void QuickStartDlgInit(struct quick_start_dlg *self);
struct quick_start_dlg *QuickStartDlgCreate(void);
void QuickStartDlgDestroy(struct quick_start_dlg **ctx_mod);
void quick_start_dlg_destroy(struct object *self);
void quick_start_dlg_on_event(struct evt_handler *self, struct cmd_event *event);
void quick_start_dlg_begin(struct modal_dialog *self, intptr_t param);
void quick_start_dlg_accept(struct modal_dialog *self, intptr_t param);

void QuickStartDlgRoll(struct quick_start_dlg *self);
