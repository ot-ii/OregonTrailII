#include "animalbase.h"

#include <stddef.h>

#include "../data/assets.h"
#include "../draw/drawable.h"
#include "../draw/mediabase.h"
#include "../base/memory.h"
#include "../draw/pictserver.h"
#include "../app/replay.h"
#include "../base/rng.h"
#include "../data/rsrc.h"
#include "../draw/server.h"
#include "../draw/sprite.h"
#include "../draw/viewsurface.h"

enum {
	HUNT_SPRITE_BASE = 16000,
};

#ifdef DIAG_BUILD
#include <stdio.h>
#include <string.h>

#include "../base/file.h"
#include "../base/str.h"

struct ab_snap {
	int32_t ah, av, l, r, seq, cel, step, dir;
	int32_t flip, wait, tag, retired, visible;
};

static void ab_snap(struct animal_base *a, struct ab_snap *s)
{
	struct spr_obj *obj = (struct spr_obj *) a;

	memset(s, 0, sizeof(*s));
	s->ah = obj->region1.p2.h;
	s->av = obj->region1.p2.v;
	s->l = obj->region1.rect.left;
	s->r = obj->region1.rect.right;
	s->seq = a->base.cur_seq;
	s->cel = obj->frame;
	s->step = a->base.cur_step;
	s->dir = a->direction;
	s->flip = obj->flip_x;
	s->wait = a->wait_until;
	s->tag = obj->tag;
	s->retired = a->retired;
	s->visible = obj->visible;
}

static void ab_log(struct animal_base *a, const char *why, const struct ab_snap *b, const struct ab_snap *n)
{
	static FILE *f;

	if (!memcmp(b, n, sizeof(*b)))
		return;
	if (!f) {
		char path[512];
		char name[128];

		FileGetExeDir(path, sizeof(path));
		FileGetExeName(name, sizeof(name));
		StrCat(path, name);
		StrCat(path, "_hunttrace.txt");
		f = fopen(path, "a");
		if (!f)
			return;
		fprintf(f, "---- trace start ----\n");
	}
	fprintf(f, "%d %p sp%d %-10s seq%d cel%d step%d  anchor %d,%d (%+d)  rect %d..%d  flip%d dir%d wait%d tag%d ret%d vis%d"
		"  bounds %d..%d\n", GameTicks(), (void *) a, a->species_id, why, n->seq, n->cel, n->step, n->ah, n->av,
		n->ah - b->ah, n->l, n->r, n->flip, n->dir, n->wait, n->tag, n->retired, n->visible, a->bounds.left,
		a->bounds.right);
	fflush(f);
}
#endif

const struct animal_base_ops ANIMAL_BASE_OPS_TABLE = {
	{
		{
			{animal_base_destroy},
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
	animal_base_set_state,
	animal_base_take_damage,
};

void AnimalBaseInit(struct animal_base *self, struct assets *server, struct view_surface *owner, int32_t kind,
	const struct rect *rects, int32_t variant)
{
	AnimSpriteInit(&self->base);
	self->base.base.base.ops = &ANIMAL_BASE_OPS_TABLE.base.base.base;

	self->bounds = RectMake(0, 0, 0, 0);
	self->server = server;

	bool huntable_k = false;
	int32_t rsrc_id_k = 0;
	int32_t variant_chance_m = 0;
	int32_t use_variant_f = 0;

	self->seq_rsrc = NULL;
	self->pos_rsrc = NULL;
	self->drawable = NULL;
	self->species_id = kind * 10;
	self->initial_facing = variant;
	huntable_k = AnimalBaseHasSecondLook(self, self->species_id);

	if (huntable_k) {
		variant_chance_m = 50;

		if (self->initial_facing)
			variant_chance_m = variant_chance_m + 35;
	}

	if (RandRangeAvg(1, 100, 1) < variant_chance_m) {
		use_variant_f = 1;

	} else {
		use_variant_f = 0;
	}

	rsrc_id_k = self->species_id + use_variant_f + HUNT_SPRITE_BASE;
	self->seq_rsrc = AssetsGetElement(server, "ASeq", rsrc_id_k, true);

	if (self->seq_rsrc)
		RsrcLoadAndUnpack(self->seq_rsrc);

	self->pos_rsrc = AssetsGetElement(server, "ACel", rsrc_id_k, true);

	if (self->pos_rsrc)
		RsrcLoadAndUnpack(self->pos_rsrc);

	self->drawable = ServerCreateVirtual(&g_pict_server->base.base, rsrc_id_k, true);
	AnimalBaseSpriteInit(self, owner, self->drawable, self->seq_rsrc, self->pos_rsrc);
	self->alive = true;
	self->damage = 0;
	self->hit_points = 1;
	self->meat_yield = 1;
	self->unused_92 = 0;
	self->band_top = rects->top;
	self->band_bottom = rects->bottom;
	self->unused_84 = false;
	self->unused_85 = false;
	self->retired = false;
	self->wait_until = 0;
	self->base.base.mask_threshold = 1;
	self->looks_variant = use_variant_f != 0;
	self->idle_roll_tick = 0;
}

struct animal_base *AnimalBaseCreate(struct assets *server, struct view_surface *owner, int32_t kind,
	const struct rect *rects, int32_t variant)
{
	struct animal_base *self = MemAlloc(1, sizeof(struct animal_base));
	AnimalBaseInit(self, server, owner, kind, rects, variant);

	return self;
}

void AnimalBaseDestroy(struct animal_base **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

bool AnimalBaseIdleRollDue(struct animal_base *self)
{
	int32_t now = GameTicks();

	if (now == self->idle_roll_tick)
		return false;

	self->idle_roll_tick = now;

	return true;
}

void AnimalBaseSpriteInit(struct animal_base *self, struct view_surface *owner, struct drawable *drawable,
	struct rsrc *seq, struct rsrc *pos)
{
	AnimSpriteInitMethod(&self->base, owner, (struct sprite *) drawable, seq, pos, false);
	self->bounds = ViewSurfaceGetBoundsV(owner);
}

void animal_base_destroy(struct object *obj)
{
	struct animal_base *self = (struct animal_base *) obj;

	if (self->drawable)
		ReleaseDrawable(&self->drawable->base.base, false);

	if (self->seq_rsrc)
		RsrcDestroy(&self->seq_rsrc);

	if (self->pos_rsrc)
		RsrcDestroy(&self->pos_rsrc);

	obj->ops = &ANIM_SPRITE_OPS_TABLE.base.base;
	anim_sprite_destroy(obj);
}

void animal_base_take_damage(struct animal_base *self, int32_t amount)
{
	self->damage = self->damage + amount;

	if (self->hit_points <= self->damage)
		self->alive = false;

	if (!self->alive)
		ANIMAL_BASE_OPS(self)->SetState(self, ANIM_DEAD);
}

static int32_t dest_left(struct animal_base *self)
{
	struct spr_obj *spr = &self->base.base;

	return self->bounds.left - RandRangeAvg(10, 200, 1) - (spr->region1.rect.right - spr->region1.rect.left);
}

static int32_t dest_right(struct animal_base *self)
{
	return self->bounds.right + RandRangeAvg(10, 200, 1);
}

static int32_t dest_band(struct animal_base *self)
{
	return RandRangeAvg(0, self->band_bottom - self->band_top, 1) + self->band_top;
}

void animal_base_tick(struct spr_obj *obj)
{
	struct animal_base *self = (struct animal_base *) obj;
	const struct animal_base_ops *ops = ANIMAL_BASE_OPS(self);
	int32_t go_right_b;
	int32_t dest_x_m;
	int32_t dest_y_k;
	int32_t passes_left_n;
#ifdef DIAG_BUILD
	struct ab_snap ab_before, ab_after;
	const char *ab_why = "step";
	ab_snap(self, &ab_before);
#endif

	if (!self->retired && self->wait_until && GameTicks() > self->wait_until) {
		self->wait_until = 0;
#ifdef DIAG_BUILD
		ab_why = "wait-over";
#endif
		go_right_b = RandRangeAvg(0, 1, 1);

		if (go_right_b) {
			if (self->direction == 1 || !obj->flip_x) {
				ops->SetState(self, ANIM_WALK_RIGHT);
				dest_x_m = dest_right(self);

			} else {
				ops->SetState(self, ANIM_WALK_LEFT);
				dest_x_m = dest_left(self);
			}

		} else {
			if (self->direction == 1 || !obj->flip_x) {
				ops->SetState(self, ANIM_WALK_LEFT);
				dest_x_m = dest_left(self);

			} else {
				ops->SetState(self, ANIM_WALK_RIGHT);
				dest_x_m = dest_right(self);
			}
		}

		dest_y_k = dest_band(self);
		SPR_OBJ_OPS(obj)->SetAnchor(obj, dest_x_m, dest_y_k);

	} else {
		anim_sprite_tick(obj);

		if (obj->region1.rect.right > self->bounds.right + 150) {
#ifdef DIAG_BUILD
			ab_why = "edge-right";
#endif
			if (self->retired) {
				ops->SetState(self, ANIM_DEAD);
				self->direction = 0;

			} else if (self->direction == 1 && self->wait_until == 0) {
				if (obj->tag > 0) {
					passes_left_n = obj->tag - 1;
					obj->tag = passes_left_n;
					ops->SetState(self, ANIM_RUN_LEFT);
					dest_x_m = self->bounds.left + RandRangeAvg(-90, -20, 1);
					dest_y_k = dest_band(self);
					SPR_OBJ_OPS(obj)->SetAnchor(obj, dest_x_m, dest_y_k);

					if (passes_left_n == 0)
						self->retired = true;

				} else {
					ops->SetState(self, ANIM_STAND);

					int32_t wait = RandRangeAvg(500, 4000, 2);
					self->wait_until = wait + GameTicks();
				}
			}

		} else if (obj->region1.rect.left < self->bounds.left - 150) {
#ifdef DIAG_BUILD
			ab_why = "edge-left";
#endif
			if (self->retired) {
				ops->SetState(self, ANIM_DEAD);
				self->direction = 0;
				self->retired = false;

			} else if (self->direction == -1 && self->wait_until == 0) {
				if (obj->tag > 0) {
					passes_left_n = obj->tag - 1;
					obj->tag = passes_left_n;
					ops->SetState(self, ANIM_RUN_RIGHT);
					dest_x_m = self->bounds.right + RandRangeAvg(20, 90, 1);
					dest_y_k = dest_band(self);
					SPR_OBJ_OPS(obj)->SetAnchor(obj, dest_x_m, dest_y_k);

					if (passes_left_n == 0)
						self->retired = true;

				} else {
					ops->SetState(self, ANIM_STAND);

					int32_t wait = RandRangeAvg(500, 4000, 2);
					self->wait_until = wait + GameTicks();
				}
			}
		}
	}
#ifdef DIAG_BUILD
	ab_snap(self, &ab_after);
	ab_log(self, ab_why, &ab_before, &ab_after);
#endif
}

bool AnimalBaseHasSecondLook(struct animal_base *self, int32_t species)
{
	(void) self;

	bool result = false;

	if ((species >= SPECIES_DEER && species <= SPECIES_ELK_FAR) || species == SPECIES_MALLARD)
		result = true;

	return result;
}

void animal_base_set_state(struct animal_base *self, int32_t state)
{
	(void) self;
	(void) state;
}
