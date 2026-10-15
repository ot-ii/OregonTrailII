#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "window.h"

struct cstr;
struct view;

struct dlg_win {
	struct window base;
	bool unused_38;
};

extern const struct window_ops DLG_WIN_OPS_TABLE;

void DlgWinInit(struct dlg_win *self, struct cstr *title, struct view *root_view, struct window *parent, bool a5);
struct dlg_win *DlgWinCreate(struct cstr *title, struct view *root_view, struct window *parent, bool a5);
void DlgWinDestroy(struct dlg_win **ctx_mod);
void dlg_win_destroy(struct object *self);

bool dlg_win_is_dialog(struct window *self);
