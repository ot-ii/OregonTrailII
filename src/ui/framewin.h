#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "window.h"

struct cstr;
struct view;
struct doc_file;

struct frame_win {
	struct window base;
	struct doc_file *inherit_flag;
	bool resizable;
	bool modal;
};

extern const struct window_ops FRAME_WIN_OPS_TABLE;

void FrameWinInit(struct frame_win *self, struct cstr *title, struct view *root_view, struct point a4,
	bool resizable, bool modal);
struct frame_win *FrameWinCreate(struct cstr *title, struct view *root_view, struct point a4, bool resizable,
	bool modal);
void FrameWinDestroy(struct frame_win **ctx_mod);
void frame_win_destroy(struct object *self);

extern struct frame_win *g_parent_win;
