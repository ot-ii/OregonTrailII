#pragma once

#include <stdint.h>

#include "window.h"

struct cstr;
struct view;

struct float_win {
	struct window base;
};

extern const struct window_ops FLOAT_WIN_OPS_TABLE;

void FloatWinInit(struct float_win *self, struct cstr *title, struct view *root_view, int32_t w, int32_t h,
	struct window *parent, int32_t a7);
struct float_win *FloatWinCreate(struct cstr *title, struct view *root_view, int32_t w, int32_t h,
	struct window *parent, int32_t a7);
void FloatWinDestroy(struct float_win **ctx_mod);
void float_win_destroy(struct object *self);

void float_win_on_deactivate(struct window *self);
void float_win_on_activate(struct window *self);
