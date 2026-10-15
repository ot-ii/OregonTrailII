#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "screendc.h"

struct canvas;
struct drawable;
struct palette;

struct offscreen_dc_ops {
	struct screen_dc_ops base;
};

struct offscreen_dc {
	struct screen_dc base;
};

#define OFFSCREEN_DC_OPS(p) ((const struct offscreen_dc_ops *) ((const struct object *) (p))->ops)

extern const struct offscreen_dc_ops OFFSCREEN_DC_OPS_TABLE;

void OffscreenDcInit(struct offscreen_dc *self);
void OffscreenDcInitCanvas(struct offscreen_dc *self, struct canvas *source, struct palette *palette_obj);
void OffscreenDcInitRect(struct offscreen_dc *self, struct rect *rect, struct palette *palette_obj, int32_t depth);
struct offscreen_dc *OffscreenDcCreate(void);
struct offscreen_dc *OffscreenDcCreateCanvas(struct canvas *source, struct palette *palette_obj);
struct offscreen_dc *OffscreenDcCreateRect(struct rect *rect, struct palette *palette_obj, int32_t depth);
void OffscreenDcDestroy(struct offscreen_dc **ctx_mod);
void offscreen_dc_destroy(struct object *self);

struct canvas *offscreen_dc_capture_rect(struct screen_dc *self, struct rect *rect);
bool offscreen_dc_is_screen(struct screen_dc *self);
void offscreen_dc_sync(struct screen_dc *self);

void OffscreenDcRender(struct offscreen_dc *self, struct drawable *obj, int32_t index, struct point *pt,
	struct rect *clip);
void OffscreenDcInitValid(struct offscreen_dc *self, void *hdc, struct rect *bounds, struct palette *palette_obj);

extern const int32_t g_rop2[8];
extern const uint32_t g_rop[8];
