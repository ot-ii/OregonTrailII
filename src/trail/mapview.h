#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "../ui/picview.h"

struct assets;
struct clip_view;
struct cmd_event;
struct game;
struct mci_video;
struct rsrc;
struct screen_dc;
struct strrsrc;

struct map_zone {
	int32_t lo;
	int32_t unused_4;
	int32_t hi;
};

enum map_label_align {
	LABEL_ALIGN_NONE = 0,
	LABEL_ALIGN_LEFT = 1,
	LABEL_ALIGN_RIGHT = 2,
	LABEL_ALIGN_CENTRE = 3,
};

struct map_view_ops {
	struct pic_view_ops base;
};

struct map_view {
	struct pic_view base;
	struct strrsrc *labels;
	struct game *game;
	struct point marker_xy[90];
	struct map_zone zones[4];
	int32_t cur_pos;
	int32_t zone;
	struct point label_pos[90];
	int32_t label_align[90];
	int32_t point_node[90];
	uint32_t point_segment[90];
	bool small_marker[90];
	int32_t inset_count;
	struct rect inset_rect[6];
	int32_t node_count;
	struct point marker;
	struct rsrc *mpvct;
	int32_t marker_pos;
	int32_t path_step_count;
	int32_t path_steps_drawn;
	int32_t cur_node;
	bool same_seg;
	bool small_marker_cur;
	uint8_t *mpvct_data;
	struct assets *lib;
	struct clip_view *pending;
	int32_t unused_592;
};

#define MAP_VIEW_OPS(p) ((const struct map_view_ops *) ((const struct object *) (p))->ops)

extern const struct map_view_ops MAP_VIEW_OPS_TABLE;

extern struct mci_video *g_map_marker;
extern int32_t g_map_marker_kept;
extern int32_t g_step_dx[8];
extern int32_t g_step_dy[8];
extern int32_t g_map_clip_count[17];
extern int32_t g_map_clip_roll;
extern int32_t g_clip_kind[96];
extern int32_t g_clip_top[96];
extern int32_t g_clip_left[96];
extern int32_t g_clip_h[96];
extern int32_t g_clip_w[96];
extern int32_t g_clip_dx[96];

void MapViewInit(struct map_view *self, int32_t view_id, const struct point *pos, int32_t map_id,
	struct game *game);
struct map_view *MapViewCreate(int32_t view_id, const struct point *pos, int32_t map_id, struct game *game);
void MapViewDestroy(struct map_view **ctx_mod);
void map_view_destroy(struct object *self);

int32_t MapViewLoadIndex(struct map_view *self, int32_t map_id);
void map_view_on_event(struct evt_handler *self, struct cmd_event *event);
void MapViewSetPending(struct map_view *self, struct clip_view *evt);
void map_view_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds);
enum map_marker_pos {
	MAP_MARKER_REDRAW = -999,
	MAP_MARKER_AT_NODE = -1,
};

void MapViewUpdateMarker(struct map_view *self, int32_t pos);
void map_view_on_idle_spare(struct evt_handler *self, void *param);
