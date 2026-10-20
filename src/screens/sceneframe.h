#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "../ui/view.h"

struct assets;
struct cmd_event;
struct inv_list;
struct rsrc;
struct strrsrc;

enum {
	SCENE_HEADER_WORDS = 16,
	SCENE_HOTSPOT_WORDS = 10,
};

enum {
	SCENE_EXIT_BACK = -2,
};

enum scene_edge {
	SCENE_EDGE_LEFT = 0,
	SCENE_EDGE_RIGHT = 1,
	SCENE_EDGE_FORWARD = 2,
	SCENE_EDGE_BACK = 3,
};

enum {
	SCENE_STORE_BLACKSMITH = 5,
	SCENE_DOCTOR = -99,
};

enum {
	SCENE_NEWSPAPER = -1,
	SCENE_ENCOUNTER_BACKDROP = -2,
};

struct scene_record {
	int32_t picture;
	int32_t exit_left;
	int32_t exit_right;
	int32_t exit_forward;
	int32_t exit_back;
	int32_t cursor_left;
	int32_t cursor_right;
	int32_t cursor_forward;
	int32_t cursor_back;
	int32_t music;
	int32_t store_list;
	struct point speaker_pos[2];
	int32_t hotspot_count;
};

struct scene_hotspot {
	struct rect rect;
	int32_t kind;
	int32_t command;
	int32_t argument;
	int32_t cursor;
};

void SceneRecordRead(const uint8_t *data, struct scene_record *out);
void SceneHotspotRead(const uint8_t *data, int32_t index, struct scene_hotspot *out);

struct scene_frame_ops {
	struct view_ops base;
};

struct scene_frame {
	struct view base;
	struct assets *data_lib;
	struct rsrc *scene_rsrc;
	struct view *child_view;
	struct inv_list *store_items;
	int32_t store_picture_id;
};

#define SCENE_FRAME_OPS(p) ((const struct scene_frame_ops *) ((const struct object *) (p))->ops)

extern const struct scene_frame_ops SCENE_FRAME_OPS_TABLE;

extern int32_t g_scene_prev;
extern int32_t g_scene_cur;
extern int32_t g_scene_view;
extern bool g_msgq_ready;
extern int32_t g_scene_music;
extern int32_t g_last_headline_idx;

void SceneFrameStartWalk(int32_t scene_id);
void SceneFrameInit(struct scene_frame *self, int32_t id, const struct point *pos, int32_t node_id);
struct scene_frame *SceneFrameCreate(int32_t id, const struct point *pos, int32_t node_id);
void SceneFrameDestroy(struct scene_frame **ctx_mod);
void scene_frame_destroy(struct object *self);

struct rsrc *SceneFrameBuildSceneView(struct scene_frame *self, int32_t node_id);
void SceneFrameBuildBackdrop(struct scene_frame *self, const struct scene_record *scene, int32_t flag);
int32_t SceneFrameEventCodeToMsgId(struct scene_frame *self, int32_t event_code);
int32_t SceneFrameHeadlineIndex(struct strrsrc *headlines, int32_t day_number);

void SceneFrameBuySupplyPackage(struct scene_frame *self);

void scene_frame_on_event(struct evt_handler *self, struct cmd_event *evt);
