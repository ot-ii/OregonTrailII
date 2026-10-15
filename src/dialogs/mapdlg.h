#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "modaldialog.h"

struct assets;
struct button;
struct clip_button;
struct drawable;
struct pic_view;
struct view;

enum map_dlg_mode {
	MAP_DLG_GUIDEBOOK = 0,
	MAP_DLG_GLOSSARY = 1,
};

struct map_zone_rec {
	int32_t start_page;
	int32_t base_image;
	int32_t end_page;
};

enum map_dlg_zone {
	MAP_DLG_ZONE_COVER = 0,
	MAP_DLG_ZONE_CONTENTS = 1,
	MAP_DLG_ZONE_ADVICE = 2,
	MAP_DLG_ZONE_ITINERARY = 3,
	MAP_DLG_ZONE_COUNT = 4,
	MAP_DLG_ZONE_NONE = 100,
};


enum map_dlg_ctl {
	GUIDE_PREV_PAGE = 1,
	GUIDE_NEXT_PAGE = 2,
	GUIDE_TAB_ADVICE = 3,
	GUIDE_TAB_ITINERARY = 4,
	GUIDE_TAB_CONTENTS = 5,
	GUIDE_FIRST_LINK = 6,
	MAP_PREV_PAGE = 101,
	MAP_NEXT_PAGE = 102,
	MAP_GLOSSARY = 103,
	MAP_START = 104,
	MAP_FIRST_LINK = 105,
};

struct map_dlg_ops {
	struct modal_dialog_ops base;
};

struct map_dlg {
	struct modal_dialog base;
	int32_t mode;
	struct view *overlay_a;
	struct view *overlay_b;
	int32_t unused_34;
	struct pic_view *picview_a;
	struct pic_view *picview_b;
	struct button *left_tab_btn;
	struct button *right_tab_btn;
	struct button *right_lower_tab_btn;
	struct button *prev_page_btn;
	struct button *next_page_btn;
	struct clip_button *landmark_btn;
	struct drawable *pending_image;
	struct assets *owner;
	struct rect left_page_rect;
	struct rect right_page_rect;
	struct rect content_rect;
	int32_t base_rsrc;
	int32_t zone_rsrc;
	int32_t cur_zone;
	int32_t landmarks[100];
	struct map_zone_rec zones[MAP_DLG_ZONE_COUNT];
	int32_t terrain;
	int32_t page_count;
	int32_t cur_page;
	int32_t zone;
	int32_t left_landmark_count;
	int32_t right_landmark_count;
	int32_t num_zones;
	int32_t unused_18A;
};

#define MAP_DLG_OPS(p) ((const struct map_dlg_ops *) ((const struct object *) (p))->ops)

extern const struct map_dlg_ops MAP_DLG_OPS_TABLE;

void MapDlgInit(struct map_dlg *self, struct assets *owner, int32_t page, int32_t year);
struct map_dlg *MapDlgCreate(struct assets *owner, int32_t page, int32_t year);
void MapDlgDestroy(struct map_dlg **ctx_mod);
void map_dlg_destroy(struct object *self);

void map_dlg_on_event(struct evt_handler *self, struct cmd_event *event);
void map_dlg_begin(struct modal_dialog *self, intptr_t param);

void MapDlgReleaseHalves(struct map_dlg *self);
void MapDlgNavCommand(struct map_dlg *self, int32_t command, int32_t current_position);
void MapDlgScrollTo(struct map_dlg *self, int32_t pos);
int32_t MapDlgFindZone(struct map_dlg *self, int32_t pos);
int32_t MapDlgTerrainTile(struct map_dlg *self, int32_t pos);
void MapDlgPlaceLandmarks(struct map_dlg *self, int32_t segment);
int32_t MapDlgLoadSegmentObjects(struct map_dlg *self, int32_t segment);
void MapDlgUpdateNavArrows(struct map_dlg *self, int32_t page);
void MapDlgSetPosition(struct map_dlg *self, int32_t pos);

extern bool g_glossary_open;
