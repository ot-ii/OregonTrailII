#pragma once

#include <stdint.h>

#include "../base/geom.h"
#include "../ui/picview.h"
#include "../draw/shape.h"

struct raft_frame;
struct rsrc;
struct screen_dc;
struct tile_map;

enum {
	MPVCT_STEPS = 10,
};

int32_t MeterMpvctStartPoint(const uint8_t *mpvct);
int32_t MeterMpvctStepCount(const uint8_t *mpvct);

struct meter_ops {
	struct pic_view_ops base;
};

struct meter {
	struct pic_view base;
	int32_t river_id;
	int32_t reverse_path;
	int32_t step_target;
	int32_t step_cur;
	int32_t origin_x;
	int32_t origin_y;
	int32_t needle_x;
	int32_t needle_y;
	int32_t range;
	int32_t value_min;
	int32_t step_count;
	struct rsrc *mpvct_rsrc;
	uint8_t *mpvct_ptr;
	struct raft_frame *raft;
	struct shape shape;
	struct rect needle_rect;
};

#define METER_OPS(p) ((const struct meter_ops *) ((const struct object *) (p))->ops)

extern const struct meter_ops METER_OPS_TABLE;

void MeterInit(struct meter *self, int32_t id, const struct point *pos, int32_t img_id, int32_t river_id,
	struct raft_frame *raft);
struct meter *MeterCreate(int32_t id, const struct point *pos, int32_t img_id, int32_t river_id,
	struct raft_frame *raft);
void MeterDestroy(struct meter **ctx_mod);
void meter_destroy(struct object *self);

void meter_on_idle_spare(struct evt_handler *self, void *task);
void meter_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds);

struct tile_map *RaftFrameGetTileMap(struct raft_frame *self);
