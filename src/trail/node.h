#pragma once

#include <stdbool.h>
#include <stdint.h>

struct canvas;
struct date;
struct game;

enum node_kind {
	NODE_KIND_START = 0,
	NODE_KIND_TRAIL = 1,
	NODE_KIND_F = 2,
	NODE_KIND_O = 3,
	NODE_KIND_END = 4,
	NODE_KIND_M = 5,
	NODE_KIND_N = 6,
	NODE_KIND_NONE = 9999,
};

enum terrain {
	TERRAIN_NORMAL = 0,
	TERRAIN_SAND = 1,
	TERRAIN_UPHILL = 2,
	TERRAIN_DOWNHILL = 3,
	TERRAIN_ROUGH = 4,
	TERRAIN_MOUNTAIN = 5,
	TERRAIN_RAFT = 6,
	TERRAIN_DESERT = 7,
};

struct node_base {
	int32_t rand_seed;
	bool npcs_selected;
	bool has_scenery_switch;
	bool scenery_switch_on;
	int32_t node_kind;

	int32_t node_id;
	int32_t cmd_set_id;
	int32_t sfx_id;
	int32_t sfx_id_snow;
	int32_t music_by_dest[4];
	int32_t scene_pic_by_season[4];
	int32_t map_id;
	int32_t price_zone;
	int32_t start_month;
	int32_t start_day;
	int32_t start_year;
	int32_t end_month;
	int32_t end_day;
	int32_t end_year;
	int32_t unread1;
	int32_t weather_region;
	int32_t game_species_count;
	int32_t game_species_ids[50];
	int32_t unread2;
	int32_t scenery_segment;
	int32_t season_backdrop_ids[6];
	int32_t link_count;
	int32_t links[8];
	int32_t event_count;
	int32_t events[20];
	int32_t conv_count;
	int32_t conv_table[70];
	int32_t inset_count;
	int32_t inset_ids[40];
	int32_t summer_inset_count;
	int32_t summer_inset_ids[20];
	int32_t snow_inset_count;
	int32_t snow_inset_ids[20];
	union { int32_t depth; int32_t steepness; };
	union { int32_t width; int32_t uphill; };
	union { int32_t swiftness; int32_t miles; };

	int32_t terrain;
};

enum crossing_pic {
	CROSSING_PIC_FERRY = 0,
	CROSSING_PIC_FERRY_TROUBLE = 1,
	CROSSING_PIC_FORD = 2,
	CROSSING_PIC_FORD_SWAMPED = 3,
	CROSSING_PIC_FORD_TIPPED = 4,
	CROSSING_PIC_FORD_STUCK = 5,
	CROSSING_PIC_FLOAT = 6,
	CROSSING_PIC_FLOAT_TROUBLE = 7,
	CROSSING_PIC_FLOAT_STUCK = 8,
	CROSSING_PIC_COUNT = 9,
};

struct encounter_slot {
	int32_t conv[2];
	int32_t tile;
	int32_t pct;
};

struct lnode {
	struct node_base base;
	struct encounter_slot encounter[8];
	int32_t ferry_price;
	int32_t toll_bridge_price;
	bool ferry_available;
	int32_t guide_conv;
	int32_t arrival_wait_days;
	int32_t ferry_wait_days;
	int32_t crossing_pics_a[CROSSING_PIC_COUNT];
	int32_t crossing_pics_b[CROSSING_PIC_COUNT];
	int32_t route_by_dest[5];
	int32_t landmark_conv;
};

struct node {
	struct node_base base;
};

extern int32_t g_conv_block_count_a;
extern int32_t g_conv_block_a[15];
extern int32_t g_conv_block_count_b;
extern int32_t g_conv_block_b[2];
extern int32_t g_conv_block_count_c;
extern int32_t g_conv_block_c[45];

void NodeBaseInit(struct node_base *self, int32_t node_id);
void NodeBaseDestroy(struct node_base **ctx_mod);
int32_t NodeBaseGetTravelMusic(struct node_base *self, struct game *game);
void NodeBaseSelectConvNpcs(struct node_base *self);
void NodeDrawTrailsideScenery(struct node *self, struct canvas *dc, int32_t *a, int32_t *b, int32_t *c);
void NodeBaseReadRecord(struct node_base *self, const uint8_t **data);
bool NodeBaseActiveOnDate(struct node_base *self, const struct date *date);
int32_t NodeBaseGetId(struct node_base *self);
int32_t NodeBaseGetArrivalSound(struct node_base *self, struct game *game);
int32_t LNodeGetLandmarkConv(struct lnode *self, struct game *game);
int32_t NodeBaseGetSeasonBackdrop(struct node_base *self, struct game *game);
int32_t NodeBaseListActiveLinks(struct node_base *self, const struct date *date, int32_t node_id, int32_t *out);
int32_t NodeInsetId(struct node *self, struct game *game);

void LNodeInit(struct lnode *self, int32_t node_id, bool apply);
struct lnode *LNodeCreate(int32_t node_id, bool apply);
void LNodeDestroy(struct lnode **ctx_mod);
int32_t LNodeSelectNextRoute(struct lnode *self, struct game *game);
struct encounter_slot *LNodeGetEncounterSlot(struct lnode *self, int32_t i);
int32_t LNodeSelectCrossingPicture(struct lnode *self, int32_t method, int32_t outcome, bool game_field_c3e);

void NodeInit(struct node *self, int32_t node_id, bool apply);
struct node *NodeCreate(int32_t node_id, bool apply);
void NodeDestroy(struct node **ctx_mod);
int32_t NodeBaseGetSegCount(struct node_base *self);
int32_t NodeGetTerrain(struct node *self);
