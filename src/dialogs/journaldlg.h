#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "modaldialog.h"

struct journal;

enum diary_ctl {
	DIARY_DATE = 100,
	DIARY_TEXT = 101,
	DIARY_NOTES = 102,
	DIARY_PREV = 103,
	DIARY_NEXT = 104,
	DIARY_DONE = 105,
};

struct journal_dlg_ops {
	struct modal_dialog_ops base;
};

struct journal_dlg {
	struct modal_dialog base;
	struct journal *page;
	int32_t text_page;
	bool text_more;
};

#define JOURNAL_DLG_OPS(p) ((const struct journal_dlg_ops *) ((const struct object *) (p))->ops)

extern const struct journal_dlg_ops JOURNAL_DLG_OPS_TABLE;

void JournalDlgInit(struct journal_dlg *self);
struct journal_dlg *JournalDlgCreate(void);
void JournalDlgDestroy(struct journal_dlg **ctx_mod);
void journal_dlg_destroy(struct object *self);
void journal_dlg_begin(struct modal_dialog *self, intptr_t param);
void JournalDlgRefresh(struct journal_dlg *self);
void journal_dlg_accept(struct modal_dialog *self, intptr_t journal);
void journal_dlg_on_event(struct evt_handler *self, struct cmd_event *event);
