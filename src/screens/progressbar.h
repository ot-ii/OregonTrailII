#pragma once

#include <stdint.h>

#include "../base/geom.h"
#include "../ui/picview.h"

struct screen_dc;
struct shape;

struct progress_bar_ops {
	struct pic_view_ops base;
};

struct progress_bar {
	struct pic_view base;
	struct shape *fill;
};

#define PROGRESS_BAR_OPS(p) ((const struct progress_bar_ops *) ((const struct object *) (p))->ops)

extern const struct progress_bar_ops PROGRESS_BAR_OPS_TABLE;

void ProgressBarInit(struct progress_bar *self, int32_t id, const struct point *pos, int32_t img_id);
struct progress_bar *ProgressBarCreate(int32_t id, const struct point *pos, int32_t img_id);
void ProgressBarDestroy(struct progress_bar **ctx_mod);
void progress_bar_destroy(struct object *self);

void progress_bar_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds);

void ProgressBarSetMarker(struct progress_bar *self, int32_t pct);
