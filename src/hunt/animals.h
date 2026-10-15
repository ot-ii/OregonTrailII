#pragma once

#include <stdint.h>

#include "animalbase.h"
#include "../base/geom.h"

struct assets;
struct view_surface;

struct animal_bird_ops {
	struct animal_base_ops base;
};

struct animal_bird {
	struct animal_base base;
};

#define ANIMAL_BIRD_OPS(p) ((const struct animal_bird_ops *) ((const struct object *) (p))->ops)

extern const struct animal_bird_ops ANIMAL_BIRD_OPS_TABLE;

void AnimalBirdInit(struct animal_bird *self, struct assets *server, struct view_surface *owner, int32_t kind,
	const struct rect *rects, int32_t variant);
struct animal_bird *AnimalBirdCreate(struct assets *server, struct view_surface *owner, int32_t kind,
	const struct rect *rects, int32_t variant);
void AnimalBirdDestroy(struct animal_bird **ctx_mod);
void animal_bird_destroy(struct object *self);

void animal_bird_set_state(struct animal_base *self, int32_t state);

struct animal_burrower_ops {
	struct animal_base_ops base;
};

struct animal_burrower {
	struct animal_base base;
};

#define ANIMAL_BURROWER_OPS(p) ((const struct animal_burrower_ops *) ((const struct object *) (p))->ops)

extern const struct animal_burrower_ops ANIMAL_BURROWER_OPS_TABLE;

void AnimalBurrowerInit(struct animal_burrower *self, struct assets *server, struct view_surface *owner,
	int32_t kind, const struct rect *rects, int32_t variant);
struct animal_burrower *AnimalBurrowerCreate(struct assets *server, struct view_surface *owner, int32_t kind,
	const struct rect *rects, int32_t variant);
void AnimalBurrowerDestroy(struct animal_burrower **ctx_mod);
void animal_burrower_destroy(struct object *self);

void animal_burrower_tick(struct spr_obj *self);
void animal_burrower_set_state(struct animal_base *self, int32_t state);

struct animal_goat_ops {
	struct animal_base_ops base;
};

struct animal_goat {
	struct animal_base base;
};

#define ANIMAL_GOAT_OPS(p) ((const struct animal_goat_ops *) ((const struct object *) (p))->ops)

extern const struct animal_goat_ops ANIMAL_GOAT_OPS_TABLE;

void AnimalGoatInit(struct animal_goat *self, struct assets *server, struct view_surface *owner, int32_t kind,
	const struct rect *rects);
struct animal_goat *AnimalGoatCreate(struct assets *server, struct view_surface *owner, int32_t kind,
	const struct rect *rects);
void AnimalGoatDestroy(struct animal_goat **ctx_mod);
void animal_goat_destroy(struct object *self);

void animal_goat_set_state(struct animal_base *self, int32_t state);

struct animal_herd_ops {
	struct animal_base_ops base;
};

struct animal_herd {
	struct animal_base base;
};

#define ANIMAL_HERD_OPS(p) ((const struct animal_herd_ops *) ((const struct object *) (p))->ops)

extern const struct animal_herd_ops ANIMAL_HERD_OPS_TABLE;

void AnimalHerdInit(struct animal_herd *self, struct assets *server, struct view_surface *owner, int32_t kind,
	const struct rect *rects, int32_t variant);
struct animal_herd *AnimalHerdCreate(struct assets *server, struct view_surface *owner, int32_t kind,
	const struct rect *rects, int32_t variant);
void AnimalHerdDestroy(struct animal_herd **ctx_mod);
void animal_herd_destroy(struct object *self);

void animal_herd_tick(struct spr_obj *self);
void animal_herd_set_state(struct animal_base *self, int32_t state);

struct animal_loner_ops {
	struct animal_base_ops base;
};

struct animal_loner {
	struct animal_base base;
};

#define ANIMAL_LONER_OPS(p) ((const struct animal_loner_ops *) ((const struct object *) (p))->ops)

extern const struct animal_loner_ops ANIMAL_LONER_OPS_TABLE;

void AnimalLonerInit(struct animal_loner *self, struct assets *server, struct view_surface *owner, int32_t kind,
	const struct rect *rects, int32_t variant);
struct animal_loner *AnimalLonerCreate(struct assets *server, struct view_surface *owner, int32_t kind,
	const struct rect *rects, int32_t variant);
void AnimalLonerDestroy(struct animal_loner **ctx_mod);
void animal_loner_destroy(struct object *self);

void animal_loner_set_state(struct animal_base *self, int32_t state);
