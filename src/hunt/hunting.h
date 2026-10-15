#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../ui/blinkview.h"
#include "../base/geom.h"

struct animal;
struct animal_base;
struct assets;
struct drawable;
struct node;
struct node_base;
struct rsrc;
struct snd_base;
struct spr_obj;

enum hunt_stat {
	HUNT_SHOTS_FIRED = 0,
	HUNT_HITS = 1,
	HUNT_WEAPON = 2,
	HUNT_SHOTS_LEFT = 3,
	HUNT_MEAT = 4,
};

struct hunting_ops {
	struct blink_view_ops base;
};

struct hunting {
	struct blink_view base;
	int32_t scenery_a_id;
	int32_t scenery_b_id;
	int32_t scenery_c_id;
	struct rsrc *shnt_rsrc;
	struct node *node;
	struct assets *data_lib;
	struct drawable *scenery_c_drawable;
	struct drawable *crosshair;
	struct spr_obj *animals[15];
	struct animal *miss_marker;
	struct rect blit_rect;
	bool ready_to_fire;
	int32_t scene_rsrc_id;
	int32_t animal_count;
	int32_t aim[5];
	bool unused_C0;
	bool is_stampede;
	struct snd_base *miss_sounds[3];
	struct snd_base *shot_sound;
	struct snd_base *dry_fire_sound;
};

#define HUNTING_OPS(p) ((const struct hunting_ops *) ((const struct object *) (p))->ops)

extern const struct hunting_ops HUNTING_OPS_TABLE;

extern struct hunting *g_hunt_state;
extern int32_t g_last_spawn_mile;
extern int32_t g_weapon_damage[4];

void HuntingInit(struct hunting *self, struct assets *data_lib, int32_t view_id, struct rect *rect,
	struct node *cnode, bool is_stampede);
struct hunting *HuntingCreate(struct assets *data_lib, int32_t view_id, struct rect *rect, struct node *cnode,
	bool is_stampede);
void HuntingDestroy(struct hunting **ctx_mod);
void hunting_destroy(struct object *self);

struct animal_base *HuntingFindHitAnimal(struct hunting *self, struct point pt, int32_t *killed);

void hunting_on_event(struct evt_handler *self, struct cmd_event *ev);

void HuntingJitterSpawnPos(struct hunting *self, struct point *pos);
void HuntingDispatchEvent2(struct hunting *self, int32_t code, int32_t param1, int32_t param2);
struct hunt_result {
	int32_t shots_fired;
	int32_t meat;
};

struct hunt_status {
	int32_t shots_left;
	int32_t weapon;
	int32_t meat;
};

void HuntingGetAim2(struct hunting *self, struct hunt_result *out);
void HuntingGetState3(struct hunting *self, struct hunt_status *out);
void HuntingSetAimXY(struct hunting *self, int32_t x, int32_t y);
void HuntingLoadScene(struct hunting *self);
void HuntingSpawnTick(struct hunting *self, struct node_base *cnode);
void HuntingClearAnimals(struct hunting *self);
int32_t HuntingSpawnSmallGame(struct hunting *self, int32_t count, const int32_t *species_list);
int32_t HuntingSpawnBigGame(struct hunting *self, int32_t count, const int32_t *species_list);
void HuntingPlayCue(struct hunting *self, int32_t sound_id);
