#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../draw/animsprite.h"
#include "../base/geom.h"

struct assets;
struct drawable;
struct rsrc;
struct view_surface;

enum hunt_species {
	SPECIES_BUFFALO = 10,
	SPECIES_BUFFALO_MID = 20,
	SPECIES_BUFFALO_FAR = 30,
	SPECIES_DEER = 40,
	SPECIES_DEER_MID = 50,
	SPECIES_DEER_FAR = 60,
	SPECIES_ANTELOPE = 70,
	SPECIES_ANTELOPE_MID = 80,
	SPECIES_ANTELOPE_FAR = 90,
	SPECIES_MOOSE = 100,
	SPECIES_MOOSE_MID = 110,
	SPECIES_MOOSE_FAR = 120,
	SPECIES_UNUSED_130 = 130,
	SPECIES_ELK = 160,
	SPECIES_ELK_MID = 170,
	SPECIES_ELK_FAR = 180,
	SPECIES_MOUNTAIN_GOAT = 190,
	SPECIES_MOUNTAIN_GOAT_MID = 200,
	SPECIES_MOUNTAIN_GOAT_FAR = 210,
	SPECIES_GRIZZLY = 220,
	SPECIES_GRIZZLY_MID = 230,
	SPECIES_GRIZZLY_FAR = 240,
	SPECIES_BLACK_BEAR = 250,
	SPECIES_BLACK_BEAR_MID = 260,
	SPECIES_BLACK_BEAR_FAR = 270,
	SPECIES_MALLARD = 280,
	SPECIES_MALLARD_MID = 290,
	SPECIES_MALLARD_FAR = 300,
	SPECIES_GOOSE = 310,
	SPECIES_GOOSE_MID = 320,
	SPECIES_GOOSE_FAR = 330,
	SPECIES_SQUIRREL = 340,
	SPECIES_SQUIRREL_MID = 350,
	SPECIES_SQUIRREL_FAR = 360,
	SPECIES_RABBIT = 370,
	SPECIES_RABBIT_MID = 380,
	SPECIES_RABBIT_FAR = 390,
	SPECIES_PRAIRIE_DOG = 400,
	SPECIES_PRAIRIE_DOG_MID = 410,
	SPECIES_PRAIRIE_DOG_FAR = 420,
	SPECIES_FIRST_SMALL_GAME = 280,
};

enum animal_state {
	ANIM_STAND = 0,
	ANIM_WALK_LEFT = 1,
	ANIM_WALK_RIGHT = 2,
	ANIM_RUN_LEFT = 3,
	ANIM_RUN_RIGHT = 4,
	ANIM_EXTRA_1 = 5,
	ANIM_EXTRA_2 = 6,
	ANIM_DEAD = 7,
};

struct animal_base;

struct animal_base_ops {
	struct anim_sprite_ops base;
	void (*SetState)(struct animal_base *self, int32_t state);
	void (*TakeDamage)(struct animal_base *self, int32_t amount);
};

struct animal_base {
	struct anim_sprite base;
	struct rect bounds;
	int32_t band_top;
	int32_t band_bottom;
	int32_t species_id;
	int32_t hit_points;
	int32_t meat_yield;
	int32_t initial_facing;
	int32_t wait_until;
	bool unused_84;
	bool unused_85;
	bool retired;
	bool looks_variant;
	struct assets *server;
	bool alive;
	bool field_8D;
	int32_t damage;
	int32_t direction;
	int32_t unused_92;
	struct rsrc *seq_rsrc;
	struct rsrc *pos_rsrc;
	struct drawable *drawable;
	int32_t idle_roll_tick;
};

#define ANIMAL_BASE_OPS(p) ((const struct animal_base_ops *) ((const struct object *) (p))->ops)

extern const struct animal_base_ops ANIMAL_BASE_OPS_TABLE;

void AnimalBaseInit(struct animal_base *self, struct assets *server, struct view_surface *owner, int32_t kind,
	const struct rect *rects, int32_t variant);
struct animal_base *AnimalBaseCreate(struct assets *server, struct view_surface *owner, int32_t kind,
	const struct rect *rects, int32_t variant);
void AnimalBaseDestroy(struct animal_base **ctx_mod);
void animal_base_destroy(struct object *self);

bool AnimalBaseIdleRollDue(struct animal_base *self);
void AnimalBaseSpriteInit(struct animal_base *self, struct view_surface *owner, struct drawable *drawable,
	struct rsrc *seq, struct rsrc *pos);

void animal_base_take_damage(struct animal_base *self, int32_t amount);
void animal_base_tick(struct spr_obj *self);

bool AnimalBaseHasSecondLook(struct animal_base *self, int32_t species);

void animal_base_set_state(struct animal_base *self, int32_t state);
