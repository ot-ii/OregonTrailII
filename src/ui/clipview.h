#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "view.h"

struct screen_dc;
struct shape;

struct clip_view_ops {
	struct view_ops base;
};

struct clip_view {
	struct view base;
	struct shape *shape;
	bool alt_draw;
};

#define CLIP_VIEW_OPS(p) ((const struct clip_view_ops *) ((const struct object *) (p))->ops)

extern const struct clip_view_ops CLIP_VIEW_OPS_TABLE;

void ClipViewInit(struct clip_view *self, int32_t id, const struct rect *rect, struct shape *shape, bool alt_draw);
struct clip_view *ClipViewCreate(int32_t id, const struct rect *rect, struct shape *shape, bool alt_draw);
void ClipViewDestroy(struct clip_view **ctx_mod);
void clip_view_destroy(struct object *self);

void clip_view_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds);

struct view *BuildRoundRectView(struct ctrl_desc *desc);
