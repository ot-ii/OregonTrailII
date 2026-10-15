#pragma once

#include <stdint.h>

#include "../app/event.h"
#include "../base/geom.h"
#include "../ui/piccontrol.h"

struct poly_ctrl;
struct screen_dc;

struct map_area_ops {
	struct pic_control_ops base;
};

struct map_area {
	struct pic_control base;
	struct poly_ctrl *outline;
};

#define MAP_AREA_OPS(p) ((const struct map_area_ops *) ((const struct object *) (p))->ops)

extern const struct map_area_ops MAP_AREA_OPS_TABLE;

void MapAreaInit(struct map_area *self, int32_t id, const struct point *pos, int32_t img_id, const struct event *cmd,
	int32_t rsrc_id, int32_t str_index);
struct map_area *MapAreaCreate(int32_t id, const struct point *pos, int32_t img_id, const struct event *cmd,
	int32_t rsrc_id, int32_t str_index);
void MapAreaDestroy(struct map_area **ctx_mod);
void map_area_destroy(struct object *self);

void map_area_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds);
