#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "view.h"

struct palette;
struct screen_dc;
struct view_surface;

struct blink_view_ops {
	struct view_ops base;
};

struct blink_view {
	struct view base;
	struct view_surface *surface;
	bool phase;
};

#define BLINK_VIEW_OPS(p) ((const struct blink_view_ops *) ((const struct object *) (p))->ops)

extern const struct blink_view_ops BLINK_VIEW_OPS_TABLE;

void BlinkViewInit(struct blink_view *self, int32_t id, const struct rect *rect, bool phase);
void BlinkViewInitSurface(struct blink_view *self, int32_t id, struct rect *rect, struct palette *palette,
	bool has_overlay, bool has_mask, bool phase);
struct blink_view *BlinkViewCreate(int32_t id, const struct rect *rect, bool phase);
struct blink_view *BlinkViewCreateSurface(int32_t id, struct rect *rect, struct palette *palette,
	bool has_overlay, bool has_mask, bool phase);
void BlinkViewDestroy(struct blink_view **ctx_mod);
void blink_view_destroy(struct object *self);

void blink_view_on_idle(struct evt_handler *self, void *task);
void blink_view_on_idle_spare(struct evt_handler *self, void *task);
void blink_view_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds);
