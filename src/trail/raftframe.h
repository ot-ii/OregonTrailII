#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../app/event.h"
#include "../base/geom.h"
#include "../ui/view.h"

struct anim_sprite;
struct assets;
struct ford_wagon;
struct palette;
struct raft_frame;
struct rsrc;
struct screen_dc;
struct sprite;
struct tile_map;

struct raft_frame_ops {
	struct view_ops base;
	void (*FindStartCell)(struct raft_frame *self, int32_t *out_x, int32_t *out_y);
	bool (*Finish)(struct raft_frame *self, int32_t result);
	int32_t (*GetProgress)(struct raft_frame *self);
	double (*GetDrift)(struct raft_frame *self);
};

struct raft_frame {
	struct view base;
	struct rsrc *aseq[11];
	struct rsrc *acel[11];
	struct rsrc *rlea[11];
	struct sprite *sprite[11];
	struct anim_sprite *decorations[100];
	struct rsrc *wrld_handle;
	struct rsrc *blck_handle;
	uint8_t *wrld_data;
	uint8_t *blck_data;
	uint32_t grid_width;
	uint32_t grid_height;
	struct ford_wagon *wagon;
	struct tile_map *tilemap;
	bool finished;
	int32_t progress;
	int32_t result;
	double drift;
};

#define RAFT_FRAME_OPS(p) ((const struct raft_frame_ops *) ((const struct object *) (p))->ops)

extern const struct raft_frame_ops RAFT_FRAME_OPS_TABLE;

void RaftFrameInit(struct raft_frame *self, int32_t view_id, struct rect *rect, struct palette *palette,
	struct assets *data_lib, int32_t map_id, int32_t wagon_rsrc, int32_t progress, int32_t speed);
struct raft_frame *RaftFrameCreate(int32_t view_id, struct rect *rect, struct palette *palette,
	struct assets *data_lib, int32_t map_id, int32_t wagon_rsrc, int32_t progress, int32_t speed);
void RaftFrameDestroy(struct raft_frame **ctx_mod);
void raft_frame_destroy(struct object *self);

void raft_frame_on_idle_spare(struct evt_handler *self, void *task);
void raft_frame_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds);
void raft_frame_find_start_cell(struct raft_frame *self, int32_t *out_x, int32_t *out_y);
bool raft_frame_finish(struct raft_frame *self, int32_t result);
int32_t raft_frame_get_progress(struct raft_frame *self);
double raft_frame_get_drift(struct raft_frame *self);

struct tile_map *RaftFrameGetTileMap(struct raft_frame *self);
