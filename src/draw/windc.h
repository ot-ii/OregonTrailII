#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "screendc.h"

struct canvas;
struct palette;

struct win_dc_ops {
	struct screen_dc_ops base;
};

struct win_dc {
	struct screen_dc base;
	bool owns_dc;
};

#define WIN_DC_OPS(p) ((const struct win_dc_ops *) ((const struct object *) (p))->ops)

extern const struct win_dc_ops WIN_DC_OPS_TABLE;

void WinDcInit(struct win_dc *self);
struct win_dc *WinDcCreate(void);
void WinDcDestroy(struct win_dc **ctx_mod);
void win_dc_destroy(struct object *self);

struct point win_dc_get_cursor_point(struct screen_dc *self);

void WinDcBlitCanvasGuarded(struct win_dc *self, struct canvas *image, struct rect *src_rect, struct rect *dst_rect,
	int32_t mode, struct rect *clip);
bool WinDcGetOwnsDc(struct win_dc *self);
void WinDcSetOwnsDc(struct win_dc *self, bool v);
bool WinDcReturnsOne(struct win_dc *self, int32_t a1, int32_t a2);
void WinDcInitDc(struct win_dc *self, void *dc, struct rect *bounds, struct palette *palette_obj, bool owns);
