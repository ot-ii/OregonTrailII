#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../app/evthandler.h"

struct view;
struct cstr;
struct dlg_win;
struct window;

enum dlg_result {
	DLG_CANCEL = -2,
	DLG_RUNNING = -1,
	DLG_OK = 1,
};

struct modal_dialog;

struct modal_dialog_ops {
	struct evt_handler_ops base;
	void (*Begin)(struct modal_dialog *self, intptr_t param);
	void (*Accept)(struct modal_dialog *self, intptr_t param);
};

struct modal_dialog {
	struct evt_handler base;
	struct view *content;
	struct cstr *title;
	int32_t result;
	bool flag_22;
	bool redraw_parent;
	struct dlg_win *dlg_win;
};

#define MODAL_DIALOG_OPS(p) ((const struct modal_dialog_ops *) ((const struct object *) (p))->ops)

extern const struct modal_dialog_ops MODAL_DIALOG_OPS_TABLE;

void ModalDialogInit(struct modal_dialog *self, int32_t id, bool flag22, bool redraw_parent);
void modal_dialog_destroy(struct object *self);
void modal_dialog_begin(struct modal_dialog *self, intptr_t param);
void modal_dialog_accept(struct modal_dialog *self, intptr_t param);

int32_t ModalDialogRun(struct modal_dialog *self, intptr_t param, struct window *parent);
void ModalDialogTrimSystemMenu(struct modal_dialog *self);

extern struct modal_dialog *g_running_dialog;
