#include "animals.h"

#include "../base/memory.h"
#include "../base/rng.h"

const struct animal_bird_ops ANIMAL_BIRD_OPS_TABLE = {
	{
		{
			{
				{animal_bird_destroy},
				spr_obj_rect_intersects,
				spr_obj_draw_masked,
				spr_obj_render,
				spr_obj_hide,
				spr_obj_show,
				spr_obj_set_image_pos,
				animal_base_tick,
				spr_obj_move_by,
				spr_obj_set_anchor,
				spr_obj_object_hit_test,
			},
			anim_sprite_position_frame,
			anim_sprite_play_sequence,
			anim_sprite_set_sequence,
			anim_sprite_get_current_sequence,
			anim_sprite_get_frame_anchor,
		},
		animal_bird_set_state,
		animal_base_take_damage,
	},
};

const struct animal_burrower_ops ANIMAL_BURROWER_OPS_TABLE = {
	{
		{
			{
				{animal_burrower_destroy},
				spr_obj_rect_intersects,
				spr_obj_draw_masked,
				spr_obj_render,
				spr_obj_hide,
				spr_obj_show,
				spr_obj_set_image_pos,
				animal_burrower_tick,
				spr_obj_move_by,
				spr_obj_set_anchor,
				spr_obj_object_hit_test,
			},
			anim_sprite_position_frame,
			anim_sprite_play_sequence,
			anim_sprite_set_sequence,
			anim_sprite_get_current_sequence,
			anim_sprite_get_frame_anchor,
		},
		animal_burrower_set_state,
		animal_base_take_damage,
	},
};

const struct animal_goat_ops ANIMAL_GOAT_OPS_TABLE = {
	{
		{
			{
				{animal_goat_destroy},
				spr_obj_rect_intersects,
				spr_obj_draw_masked,
				spr_obj_render,
				spr_obj_hide,
				spr_obj_show,
				spr_obj_set_image_pos,
				animal_base_tick,
				spr_obj_move_by,
				spr_obj_set_anchor,
				spr_obj_object_hit_test,
			},
			anim_sprite_position_frame,
			anim_sprite_play_sequence,
			anim_sprite_set_sequence,
			anim_sprite_get_current_sequence,
			anim_sprite_get_frame_anchor,
		},
		animal_goat_set_state,
		animal_base_take_damage,
	},
};

const struct animal_herd_ops ANIMAL_HERD_OPS_TABLE = {
	{
		{
			{
				{animal_herd_destroy},
				spr_obj_rect_intersects,
				spr_obj_draw_masked,
				spr_obj_render,
				spr_obj_hide,
				spr_obj_show,
				spr_obj_set_image_pos,
				animal_herd_tick,
				spr_obj_move_by,
				spr_obj_set_anchor,
				spr_obj_object_hit_test,
			},
			anim_sprite_position_frame,
			anim_sprite_play_sequence,
			anim_sprite_set_sequence,
			anim_sprite_get_current_sequence,
			anim_sprite_get_frame_anchor,
		},
		animal_herd_set_state,
		animal_base_take_damage,
	},
};

const struct animal_loner_ops ANIMAL_LONER_OPS_TABLE = {
	{
		{
			{
				{animal_loner_destroy},
				spr_obj_rect_intersects,
				spr_obj_draw_masked,
				spr_obj_render,
				spr_obj_hide,
				spr_obj_show,
				spr_obj_set_image_pos,
				animal_base_tick,
				spr_obj_move_by,
				spr_obj_set_anchor,
				spr_obj_object_hit_test,
			},
			anim_sprite_position_frame,
			anim_sprite_play_sequence,
			anim_sprite_set_sequence,
			anim_sprite_get_current_sequence,
			anim_sprite_get_frame_anchor,
		},
		animal_loner_set_state,
		animal_base_take_damage,
	},
};

static struct spr_obj *spr(struct animal_base *self)
{
	return &self->base.base;
}

static void play(struct animal_base *self, int32_t frame, int32_t seq)
{
	if (frame >= 0 && self->base.cur_seq != frame)
		ANIM_SPRITE_OPS(self)->PlaySequence(&self->base, frame, seq);
}

static void walk_off(struct animal_base *self, int32_t facing, int32_t range,
	void (*set_state)(struct animal_base *self, int32_t state))
{
	struct spr_obj *obj = spr(self);
	int32_t dest_x_j = 0;

	switch (facing) {
		case 1:
			set_state(self, ANIM_WALK_LEFT);
			dest_x_j = self->bounds.left - RandRangeAvg(10, range, 1) -
				(obj->region1.rect.right - obj->region1.rect.left);
			break;
		case 2:
			set_state(self, ANIM_WALK_RIGHT);
			dest_x_j = self->bounds.right + RandRangeAvg(10, range, 1);
			break;
	}

	int32_t y_range_o = self->band_bottom - self->band_top;
	int32_t anchor_y_c = RandRangeAvg(0, y_range_o, 1) + self->band_top;
	spr_obj_set_anchor(obj, dest_x_j, anchor_y_c);
}

static int32_t pick_facing(struct animal_base *self)
{
	int32_t facing = self->initial_facing;

	if (!facing)
		facing = RandRangeAvg(1, 2, 1);

	if (facing != 2)
		facing = 1;

	return facing;
}

void AnimalBirdInit(struct animal_bird *self, struct assets *server, struct view_surface *owner, int32_t kind,
	const struct rect *rects, int32_t variant)
{
	struct animal_base *b = &self->base;

	AnimalBaseInit(b, server, owner, kind, rects, variant);
	b->base.base.base.ops = &ANIMAL_BIRD_OPS_TABLE.base.base.base.base;

	switch (b->species_id) {
		case SPECIES_MALLARD:
		case SPECIES_MALLARD_MID:
		case SPECIES_MALLARD_FAR:
			b->meat_yield = RandRangeAvg(2, 5, 1);
			b->hit_points = RandRangeAvg(10, 35, 1);
			break;
		case SPECIES_GOOSE:
		case SPECIES_GOOSE_MID:
		case SPECIES_GOOSE_FAR:
			b->meat_yield = RandRangeAvg(5, 10, 1);
			b->hit_points = RandRangeAvg(15, 40, 1);
			break;
	}

	walk_off(b, pick_facing(b), 200, animal_bird_set_state);
}

struct animal_bird *AnimalBirdCreate(struct assets *server, struct view_surface *owner, int32_t kind,
	const struct rect *rects, int32_t variant)
{
	struct animal_bird *self = MemAlloc(1, sizeof(struct animal_bird));
	AnimalBirdInit(self, server, owner, kind, rects, variant);

	return self;
}

void AnimalBirdDestroy(struct animal_bird **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void animal_bird_destroy(struct object *self)
{
	self->ops = &ANIMAL_BASE_OPS_TABLE.base.base.base;
	animal_base_destroy(self);
}

void animal_bird_set_state(struct animal_base *self, int32_t state)
{
	int32_t seq = -1;
	int32_t frame = -1;

	switch (state) {
		case ANIM_WALK_LEFT:
			self->direction = 1;
			spr(self)->flip_x = false;
			seq = RandRangeAvg(0, 5, 1);
			frame = 0;
			break;
		case ANIM_WALK_RIGHT:
			self->direction = -1;
			spr(self)->flip_x = true;
			seq = RandRangeAvg(0, 5, 1);
			frame = 0;
			break;
		case ANIM_DEAD:
			frame = 1;
			break;
		case ANIM_STAND:
			frame = 2;
			break;
		default:
			break;
	}

	play(self, frame, seq);
}

void AnimalBurrowerInit(struct animal_burrower *self, struct assets *server, struct view_surface *owner,
	int32_t kind, const struct rect *rects, int32_t variant)
{
	struct animal_base *b = &self->base;

	AnimalBaseInit(b, server, owner, kind, rects, variant);
	b->base.base.base.ops = &ANIMAL_BURROWER_OPS_TABLE.base.base.base.base;

	switch (b->species_id) {
		case SPECIES_SQUIRREL:
		case SPECIES_SQUIRREL_MID:
		case SPECIES_SQUIRREL_FAR:
			b->meat_yield = RandRangeAvg(1, 1, 1);
			b->hit_points = RandRangeAvg(10, 35, 1);
			break;
		case SPECIES_PRAIRIE_DOG:
		case SPECIES_PRAIRIE_DOG_MID:
		case SPECIES_PRAIRIE_DOG_FAR:
			b->meat_yield = RandRangeAvg(1, 1, 1);
			b->hit_points = RandRangeAvg(10, 35, 1);
			break;
	}

	walk_off(b, pick_facing(b), 200, animal_burrower_set_state);
}

struct animal_burrower *AnimalBurrowerCreate(struct assets *server, struct view_surface *owner, int32_t kind,
	const struct rect *rects, int32_t variant)
{
	struct animal_burrower *self = MemAlloc(1, sizeof(struct animal_burrower));
	AnimalBurrowerInit(self, server, owner, kind, rects, variant);

	return self;
}

void AnimalBurrowerDestroy(struct animal_burrower **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void animal_burrower_destroy(struct object *self)
{
	self->ops = &ANIMAL_BASE_OPS_TABLE.base.base.base;
	animal_base_destroy(self);
}

void animal_burrower_tick(struct spr_obj *obj)
{
	struct animal_base *self = (struct animal_base *) obj;

	animal_base_tick(obj);

	if (self->base.cur_seq == 0) {
		if (AnimalBaseIdleRollDue(self) && RandRangeAvg(0, 2000, 1) == 10)
			ANIMAL_BASE_OPS(self)->SetState(self, ANIM_STAND);
	}
}

void animal_burrower_set_state(struct animal_base *self, int32_t state)
{
	int32_t seq = -1;
	int32_t frame = -1;

	switch (state) {
		case ANIM_WALK_LEFT:
			self->direction = 1;
			spr(self)->flip_x = false;
			frame = 0;
			seq = RandRangeAvg(2, 6, 1);
			break;
		case ANIM_WALK_RIGHT:
			self->direction = -1;
			spr(self)->flip_x = true;
			seq = RandRangeAvg(2, 6, 1);
			frame = 0;
			break;
		case ANIM_DEAD:
			frame = 1;
			break;
		case ANIM_STAND:
			frame = 3;
			break;
		default:
			break;
	}

	play(self, frame, seq);
}

void AnimalGoatInit(struct animal_goat *self, struct assets *server, struct view_surface *owner, int32_t kind,
	const struct rect *rects)
{
	struct animal_base *b = &self->base;
	struct spr_obj *obj = spr(b);
	int32_t target_h_f = 0;

	AnimalBaseInit(b, server, owner, kind, rects, 0);
	b->base.base.base.ops = &ANIMAL_GOAT_OPS_TABLE.base.base.base.base;

	b->meat_yield = RandRangeAvg(25, 115, 1);
	b->hit_points = RandRangeAvg(40, 65, 1);

	switch (RandRangeAvg(0, 1, 1)) {
		case 0:
			animal_goat_set_state(b, ANIM_WALK_LEFT);
			target_h_f = b->bounds.left - RandRangeAvg(10, 200, 1) - (obj->region1.rect.right - obj->region1.rect.left);
			break;
		case 1:
			animal_goat_set_state(b, ANIM_WALK_RIGHT);
			target_h_f = RandRangeAvg(10, 200, 1) + b->bounds.right;
			break;
	}

	int32_t band_height_b = b->band_bottom - b->band_top;
	int32_t target_v_c = RandRangeAvg(0, band_height_b, 1) + b->band_top;
	spr_obj_set_anchor(obj, target_h_f, target_v_c);

	b->unused_84 = true;
}

struct animal_goat *AnimalGoatCreate(struct assets *server, struct view_surface *owner, int32_t kind,
	const struct rect *rects)
{
	struct animal_goat *self = MemAlloc(1, sizeof(struct animal_goat));
	AnimalGoatInit(self, server, owner, kind, rects);

	return self;
}

void AnimalGoatDestroy(struct animal_goat **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void animal_goat_destroy(struct object *self)
{
	self->ops = &ANIMAL_BASE_OPS_TABLE.base.base.base;
	animal_base_destroy(self);
}

void animal_goat_set_state(struct animal_base *self, int32_t state)
{
	int32_t frame_duration_p = -1;
	int32_t frame_index_e = -1;

	switch (state) {
		case ANIM_STAND:
			frame_index_e = 4;
			break;
		case ANIM_WALK_LEFT:
			self->direction = 1;
			spr(self)->flip_x = false;
			frame_index_e = 0;
			break;
		case ANIM_WALK_RIGHT:
			self->direction = -1;
			spr(self)->flip_x = true;
			frame_index_e = 0;
			break;
		case ANIM_DEAD:
			frame_index_e = 1;
			break;
		case ANIM_EXTRA_1:
			frame_index_e = 3;
			break;
		default:
			break;
	}

	play(self, frame_index_e, frame_duration_p);
}

void AnimalHerdInit(struct animal_herd *self, struct assets *server, struct view_surface *owner, int32_t kind,
	const struct rect *rects, int32_t variant)
{
	struct animal_base *b = &self->base;

	AnimalBaseInit(b, server, owner, kind, rects, variant);
	b->base.base.base.ops = &ANIMAL_HERD_OPS_TABLE.base.base.base.base;

	switch (b->species_id) {
		case SPECIES_BUFFALO:
		case SPECIES_BUFFALO_MID:
		case SPECIES_BUFFALO_FAR:
			b->meat_yield = RandRangeAvg(200, 500, 1);
			b->hit_points = RandRangeAvg(75, 100, 1);
			break;
		case SPECIES_ANTELOPE:
		case SPECIES_ANTELOPE_MID:
		case SPECIES_ANTELOPE_FAR:
			b->meat_yield = RandRangeAvg(20, 50, 1);

			if (!b->field_8D)
				b->meat_yield = RandRangeAvg(15, 40, 1) + b->meat_yield;

			b->hit_points = RandRangeAvg(40, 65, 1);
			break;
		case SPECIES_DEER:
		case SPECIES_DEER_MID:
		case SPECIES_DEER_FAR:
			b->meat_yield = RandRangeAvg(15, 150, 1);

			if (!b->field_8D)
				b->meat_yield = RandRangeAvg(15, 40, 1) + b->meat_yield;

			b->hit_points = RandRangeAvg(40, 65, 1);
			break;
		case SPECIES_ELK:
		case SPECIES_ELK_MID:
		case SPECIES_ELK_FAR:
			b->meat_yield = RandRangeAvg(100, 225, 1);

			if (!b->field_8D)
				b->meat_yield = RandRangeAvg(15, 40, 1) + b->meat_yield;

			b->hit_points = RandRangeAvg(55, 80, 1);
			break;
	}

	walk_off(b, pick_facing(b), 400, animal_herd_set_state);

	b->unused_84 = true;

	if (kind >= 1 && kind <= 3)
		b->unused_85 = true;
}

struct animal_herd *AnimalHerdCreate(struct assets *server, struct view_surface *owner, int32_t kind,
	const struct rect *rects, int32_t variant)
{
	struct animal_herd *self = MemAlloc(1, sizeof(struct animal_herd));
	AnimalHerdInit(self, server, owner, kind, rects, variant);

	return self;
}

void AnimalHerdDestroy(struct animal_herd **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void animal_herd_destroy(struct object *self)
{
	self->ops = &ANIMAL_BASE_OPS_TABLE.base.base.base;
	animal_base_destroy(self);
}

void animal_herd_tick(struct spr_obj *obj)
{
	struct animal_base *self = (struct animal_base *) obj;

	animal_base_tick(obj);

	if (self->base.cur_seq == 0) {
		if (AnimalBaseIdleRollDue(self) && RandRangeAvg(0, 2000, 2) == 10)
			ANIMAL_BASE_OPS(self)->SetState(self, ANIM_EXTRA_1);

	} else if (self->base.cur_seq == 4) {
		if (AnimalBaseIdleRollDue(self) && RandRangeAvg(0, 2500, 2) == 10)
			ANIMAL_BASE_OPS(self)->SetState(self, ANIM_EXTRA_2);
	}
}

void animal_herd_set_state(struct animal_base *self, int32_t state)
{
	int32_t seq = -1;
	int32_t frame = -1;

	switch (state) {
		case ANIM_STAND:
			frame = 5;
			break;
		case ANIM_RUN_LEFT:
			self->direction = 1;
			spr(self)->flip_x = false;
			frame = 1;
			seq = RandRangeAvg(0, 3, 1);
			break;
		case ANIM_RUN_RIGHT:
			self->direction = -1;
			spr(self)->flip_x = true;
			frame = 1;
			seq = RandRangeAvg(0, 3, 1);
			break;
		case ANIM_WALK_LEFT:
			self->direction = 1;
			spr(self)->flip_x = false;
			frame = 0;
			seq = RandRangeAvg(0, 8, 1);
			break;
		case ANIM_WALK_RIGHT:
			self->direction = -1;
			spr(self)->flip_x = true;
			seq = RandRangeAvg(0, 8, 1);
			frame = 0;
			break;
		case ANIM_DEAD:
			frame = 2;
			break;
		case ANIM_EXTRA_1:
			frame = 4;
			break;
		case ANIM_EXTRA_2:
			if (self->species_id == SPECIES_BUFFALO || self->species_id == SPECIES_BUFFALO_MID ||
				self->species_id == SPECIES_BUFFALO_FAR) {
				frame = 6;
				seq = RandRangeAvg(30, 70, 1);
			}
			break;
		default:
			break;
	}

	play(self, frame, seq);
}

void AnimalLonerInit(struct animal_loner *self, struct assets *server, struct view_surface *owner, int32_t kind,
	const struct rect *rects, int32_t variant)
{
	struct animal_base *b = &self->base;

	AnimalBaseInit(b, server, owner, kind, rects, variant);
	b->base.base.base.ops = &ANIMAL_LONER_OPS_TABLE.base.base.base.base;

	switch (b->species_id) {
		case SPECIES_RABBIT:
		case SPECIES_RABBIT_MID:
		case SPECIES_RABBIT_FAR:
			b->meat_yield = RandRangeAvg(1, 4, 1);
			b->hit_points = RandRangeAvg(15, 40, 1);
			break;
		case SPECIES_MOOSE:
		case SPECIES_MOOSE_MID:
		case SPECIES_MOOSE_FAR:
			b->meat_yield = RandRangeAvg(150, 350, 2);

			if (!b->field_8D)
				b->meat_yield = RandRangeAvg(30, 90, 1) + b->meat_yield;

			b->hit_points = RandRangeAvg(60, 85, 1);
			break;
		case SPECIES_GRIZZLY:
		case SPECIES_GRIZZLY_MID:
		case SPECIES_GRIZZLY_FAR:
			b->meat_yield = RandRangeAvg(75, 300, 1);
			b->hit_points = RandRangeAvg(75, 100, 1);
			break;
		case SPECIES_BLACK_BEAR:
		case SPECIES_BLACK_BEAR_MID:
		case SPECIES_BLACK_BEAR_FAR:
			b->meat_yield = RandRangeAvg(50, 175, 1);
			b->hit_points = RandRangeAvg(70, 95, 1);
			break;
	}

	walk_off(b, pick_facing(b), 200, animal_loner_set_state);
}

struct animal_loner *AnimalLonerCreate(struct assets *server, struct view_surface *owner, int32_t kind,
	const struct rect *rects, int32_t variant)
{
	struct animal_loner *self = MemAlloc(1, sizeof(struct animal_loner));
	AnimalLonerInit(self, server, owner, kind, rects, variant);

	return self;
}

void AnimalLonerDestroy(struct animal_loner **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void animal_loner_destroy(struct object *self)
{
	self->ops = &ANIMAL_BASE_OPS_TABLE.base.base.base;
	animal_base_destroy(self);
}

void animal_loner_set_state(struct animal_base *self, int32_t state)
{
	int32_t seq = -1;
	int32_t frame = -1;

	switch (state) {
		case ANIM_STAND:
			frame = 4;
			break;
		case ANIM_RUN_LEFT:
			self->direction = 1;
			spr(self)->flip_x = false;
			frame = 1;
			break;
		case ANIM_RUN_RIGHT:
			self->direction = -1;
			spr(self)->flip_x = true;
			frame = 1;
			break;
		case ANIM_WALK_LEFT:
			self->direction = 1;
			spr(self)->flip_x = false;
			frame = 0;
			break;
		case ANIM_WALK_RIGHT:
			self->direction = -1;
			spr(self)->flip_x = true;
			frame = 0;
			break;
		case ANIM_DEAD:
			frame = 2;
			break;
		default:
			break;
	}

	play(self, frame, seq);
}
