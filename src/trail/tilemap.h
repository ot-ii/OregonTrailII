#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "../draw/viewsurface.h"

struct coll_grid;
struct palette;
struct rect_rgn_list;
struct tile_map;
struct win_dc;

enum raft_cell {
	RAFT_WATER = 0,
	RAFT_LAND = 1,
	RAFT_CURRENT_FIRST = 2,
	RAFT_CURRENT_LAST = 33,
	RAFT_WHIRLPOOL = 34,
	RAFT_RAPIDS_1 = 35,
	RAFT_RAPIDS_2 = 36,
	RAFT_ROCK_1 = 39,
	RAFT_ROCK_2 = 40,
	RAFT_ROCK_3 = 41,
	RAFT_ROCK_4 = 42,
	RAFT_START = 43,
	RAFT_SHORE_FIRST = 44,
};

struct tile_map_ops {
	struct view_surface_ops base;
	void (*ScrollRedraw)(struct tile_map *self, struct rect_rgn_list *region, bool use_dirty_buffer);
	void (*DispatchToChildViews)(struct tile_map *self, int32_t dh, int32_t dv);
	void (*ScrollTo)(struct tile_map *self, int32_t h, int32_t v);
	void (*BlitOrInvalidate)(struct tile_map *self, struct win_dc *windc, struct rect_rgn_list *rgn,
		bool invalidate_only);
};

struct tile_map {
	struct view_surface base;
	struct rect map_rect;
	struct point map_origin;
	struct point scroll_pos;
	const uint8_t *tile_sheet;
	const uint8_t *map_data;
	int32_t first_shore_tile;
	int32_t *tiles;
	int32_t *cells4;
	int32_t cw4;
	int32_t ch4;
	bool scroll_dirty;
	bool use_dirty_buffer;
	struct point last_scroll;
	struct coll_grid *grid;
};

#define TILE_MAP_OPS(p) ((const struct tile_map_ops *) ((const struct object *) (p))->ops)

extern const struct tile_map_ops TILE_MAP_OPS_TABLE;

void TileMapInit(struct tile_map *self, struct rect *bounds, struct palette *palette, const struct rect *src_rect,
	const uint8_t *map_data, const uint8_t *tile_sheet, int32_t first_shore_tile);
struct tile_map *TileMapCreate(struct rect *bounds, struct palette *palette, const struct rect *src_rect,
	const uint8_t *map_data, const uint8_t *tile_sheet, int32_t first_shore_tile);
void TileMapDestroy(struct tile_map **ctx_mod);
void tile_map_destroy(struct object *self);

void tile_map_blit(struct view_surface *self, struct rect *rect);
void tile_map_scroll_redraw(struct tile_map *self, struct rect_rgn_list *region, bool use_dirty_buffer);
void tile_map_blit_or_invalidate(struct tile_map *self, struct win_dc *windc, struct rect_rgn_list *rgn,
	bool invalidate_only);
void tile_map_render_pipeline(struct view_surface *self, struct win_dc *window, bool flush);
void tile_map_dispatch_to_child_views(struct tile_map *self, int32_t dh, int32_t dv);
void tile_map_scroll_to(struct tile_map *self, int32_t h, int32_t v);
