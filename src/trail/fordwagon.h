#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../draw/animsprite.h"
#include "../base/geom.h"

struct assets;
struct ford_wagon;
struct raft_frame;
struct rsrc;
struct sprite;
struct view_surface;

enum ford_frames {
	FORD_FRAMES_SPIN_A = 0,
	FORD_FRAMES_HEADINGS = 1,
	FORD_FRAMES_SPIN_B = 2,
	FORD_FRAMES_COUNT = 3,
};

struct ford_wagon_ops {
	struct anim_sprite_ops base;
	struct view_surface *(*GetRootView)(struct ford_wagon *self);
	void (*SetHeading)(struct ford_wagon *self, int32_t heading);
	bool (*PhysicsStep)(struct ford_wagon *self, int32_t direction, int32_t speed, int32_t target_v,
		int32_t target_h);
};

struct ford_wagon {
	struct anim_sprite base;
	int32_t heading;
	double velocity;
	double drift_h;
	double drift_v;
	int32_t phys_tick;
	int32_t offset_h;
	int32_t offset_v;
	int32_t drift_gain;
	int32_t damage;
	int32_t spin_risk;
	uint32_t grid_w;
	uint32_t grid_h;
	const uint8_t *tile_sheet;
	const uint8_t *grid;
	struct rsrc *acel[FORD_FRAMES_COUNT];
	struct rsrc *rlea[FORD_FRAMES_COUNT];
	struct sprite *sprite[FORD_FRAMES_COUNT];
	struct raft_frame *parent;
	int32_t anim_count;
	int32_t anim_frame;
	int32_t anim_step;
	int32_t anim_target;
	int32_t anim_delay;
	int32_t anim_next;
};

#define FORD_WAGON_OPS(p) ((const struct ford_wagon_ops *) ((const struct object *) (p))->ops)

extern const struct ford_wagon_ops FORD_WAGON_OPS_TABLE;

extern int32_t g_current_angle[8];
extern double g_pi;
extern void (*g_raft_pilot)(struct ford_wagon *raft);

void FordWagonInit(struct ford_wagon *self, struct view_surface *owner, struct assets *data_lib, uint32_t rsrc_id,
	struct raft_frame *parent);
struct ford_wagon *FordWagonCreate(struct view_surface *owner, struct assets *data_lib, uint32_t rsrc_id,
	struct raft_frame *parent);
void FordWagonDestroy(struct ford_wagon **ctx_mod);
void ford_wagon_destroy(struct object *self);

void ford_wagon_tick(struct spr_obj *self);
void ford_wagon_move_by(struct spr_obj *self, int32_t dx, int32_t dy);
struct view_surface *ford_wagon_get_root_view(struct ford_wagon *self);
void ford_wagon_set_heading(struct ford_wagon *self, int32_t heading);
bool ford_wagon_physics_step(struct ford_wagon *self, int32_t direction, int32_t speed, int32_t target_v,
	int32_t target_h);

void FordWagonSetTurnAnim(struct ford_wagon *self, int32_t *heading);
bool FordWagonCanMove(struct ford_wagon *self, int32_t dx, int32_t dy);
bool FordWagonReachedBank(struct ford_wagon *self);
bool FordWagonCheckCollision(struct ford_wagon *self);
