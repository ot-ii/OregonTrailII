#include "fordwagon.h"

#include <math.h>
#include <stddef.h>

#include "../app/app.h"
#include "../data/assets.h"
#include "../base/byteorder.h"
#include "../base/memory.h"
#include "../app/otiiapp.h"
#include "raftframe.h"
#include "../app/replay.h"
#include "../base/rng.h"
#include "../data/rsrc.h"
#include "../draw/sprite.h"
#include "tilemap.h"
#include "../ui/view.h"
#include "../draw/viewsurface.h"
#include "../ui/window.h"

static int32_t g_ford_last_tick;

int32_t g_current_angle[8] = {
	90, 45, 0, 315, 270, 225, 180, 135,
};

static const double MATH_PI = 3.14159827;

double g_pi = 3.14159827 * 2.0;

void (*g_raft_pilot)(struct ford_wagon *raft);

const struct ford_wagon_ops FORD_WAGON_OPS_TABLE = {
	{
		{
			{ford_wagon_destroy},
			spr_obj_rect_intersects,
			spr_obj_draw_masked,
			spr_obj_render,
			spr_obj_hide,
			spr_obj_show,
			spr_obj_set_image_pos,
			ford_wagon_tick,
			ford_wagon_move_by,
			spr_obj_set_anchor,
			spr_obj_object_hit_test,
		},
		anim_sprite_position_frame,
		anim_sprite_play_sequence,
		anim_sprite_set_sequence,
		anim_sprite_get_current_sequence,
		anim_sprite_get_frame_anchor,
	},
	ford_wagon_get_root_view,
	ford_wagon_set_heading,
	ford_wagon_physics_step,
};

static struct spr_obj *spr(struct ford_wagon *self)
{
	return &self->base.base;
}

static struct tile_map *root_map(struct ford_wagon *self)
{
	return (struct tile_map *) FORD_WAGON_OPS(self)->GetRootView(self);
}

static uint32_t grid_cell(struct ford_wagon *self, int32_t index)
{
	return ReadLeU16(self->grid + 4 + 2 * (intptr_t) index);
}

static void add_view(struct rect *r, struct tile_map *v)
{
	r->left = r->left + v->scroll_pos.h;
	r->right = r->right + v->scroll_pos.h;
	r->top = r->top + v->scroll_pos.v;
	r->bottom = r->bottom + v->scroll_pos.v;
}

static void to_cells(struct rect *r)
{
	r->top /= 16;
	r->left /= 16;

	if (r->bottom & 0x000F)
		r->bottom += 16;

	r->bottom /= 16;

	if (r->right & 0x000F)
		r->right += 16;

	r->right /= 16;
}

static void clip_to_grid(struct ford_wagon *self, struct rect *r)
{
	if (r->left < 0)
		r->left = 0;

	if (r->top < 0)
		r->top = 0;

	if (r->right > (int32_t) self->grid_w)
		r->right = (int32_t) self->grid_w;

	if (r->bottom > (int32_t) self->grid_h)
		r->bottom = (int32_t) self->grid_h;
}

static void scroll_to_raft(struct ford_wagon *self, struct point hot_spot)
{
	struct tile_map *map = root_map(self);
	struct rect view_bounds = ViewSurfaceGetBoundsV(&map->base);
	int32_t half_width = (view_bounds.right - view_bounds.left) / 2;
	int32_t scroll_down = 3 * ((view_bounds.bottom - view_bounds.top) / 4);

	TILE_MAP_OPS(map)->ScrollTo(map, hot_spot.h - half_width, hot_spot.v - scroll_down);
}

void FordWagonInit(struct ford_wagon *self, struct view_surface *owner, struct assets *data_lib, uint32_t rsrc_id,
	struct raft_frame *parent)
{
	AnimSpriteInitSeq(&self->base, owner, NULL, NULL, NULL, false);
	self->base.base.base.ops = &FORD_WAGON_OPS_TABLE.base.base.base;

	self->parent = parent;
	self->tile_sheet = ((struct tile_map *) spr(self)->owner)->tile_sheet;
	self->grid = ((struct tile_map *) spr(self)->owner)->map_data;
	self->grid_w = ReadLeU16(self->grid);
	self->grid_h = ReadLeU16(self->grid + 2);

	for (int32_t frame_set_a = 0; frame_set_a < FORD_FRAMES_COUNT; frame_set_a++) {
		self->acel[frame_set_a] = AssetsGetElement(data_lib, "ACEL", rsrc_id + frame_set_a, true);
		RsrcLoadAndUnpack(self->acel[frame_set_a]);
		self->rlea[frame_set_a] = AssetsGetElement(data_lib, "RLEA", rsrc_id + frame_set_a, true);
		self->sprite[frame_set_a] = SpriteCreate(self->rlea[frame_set_a]);
		SPRITE_OPS(self->sprite[frame_set_a])->ByteswapResource(self->sprite[frame_set_a]);
	}

	spr(self)->image = self->sprite[FORD_FRAMES_HEADINGS];
	self->anim_count = 0;
	anim_sprite_set_sequence(&self->base, NULL, self->acel[FORD_FRAMES_SPIN_A], false);

	self->heading = 270;
	self->drift_h = 0;
	self->drift_v = 0;
	self->drift_gain = 8;
	self->velocity = 0;
	self->damage = 0;
	self->spin_risk = 0;
}

struct ford_wagon *FordWagonCreate(struct view_surface *owner, struct assets *data_lib, uint32_t rsrc_id,
	struct raft_frame *parent)
{
	struct ford_wagon *self = MemAlloc(1, sizeof(struct ford_wagon));
	FordWagonInit(self, owner, data_lib, rsrc_id, parent);

	return self;
}

void FordWagonDestroy(struct ford_wagon **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void ford_wagon_destroy(struct object *obj)
{
	struct ford_wagon *self = (struct ford_wagon *) obj;

	for (int32_t frame_set_a = 0; frame_set_a < FORD_FRAMES_COUNT; frame_set_a++) {
		SpriteDestroy(&self->sprite[frame_set_a]);
		RsrcDestroy(&self->acel[frame_set_a]);
		RsrcDestroy(&self->rlea[frame_set_a]);
	}

	obj->ops = &ANIM_SPRITE_OPS_TABLE.base.base;
	anim_sprite_destroy(obj);
}

static void steer(struct ford_wagon *self, int32_t speed_k, bool *stop)
{
	struct spr_obj *obj = spr(self);
	int32_t turn = 0;
	struct window *window_c = self->parent->base.owner_window;

	if (!window_c) {
		*stop = true;
		return;
	}

	if (g_raft_pilot)
		g_raft_pilot(self);

	struct point cursor_pt_i = WindowCursorToClient(window_c);
	struct point frame_origin_n = ViewGetOrigin(&self->parent->base);
	cursor_pt_i.h = cursor_pt_i.h - frame_origin_n.h;
	cursor_pt_i.v = cursor_pt_i.v - frame_origin_n.v;

	struct point hot_spot_p = obj->region1.p2;
	struct tile_map *map = root_map(self);
	hot_spot_p.h = map->scroll_pos.h + hot_spot_p.h;
	hot_spot_p.v = map->scroll_pos.v + hot_spot_p.v;
	scroll_to_raft(self, hot_spot_p);

	double delta_h_m = (double) (cursor_pt_i.h - obj->region1.p2.h);
	double delta_v_n = (double) (cursor_pt_i.v - obj->region1.p2.v);

	if (delta_h_m == 0.0)
		delta_h_m = 9.999999999999999e-35;

	double angle_n = atan(delta_v_n / delta_h_m);

	if (delta_h_m < 0.0)
		angle_n = angle_n + MATH_PI;

	double target_heading_g = angle_n / g_pi * 360.0;

	if (target_heading_g < 0.0)
		target_heading_g = (target_heading_g + 90.0) + 270.0;

	int32_t heading_diff_l = (int32_t) (target_heading_g - self->heading);
	heading_diff_l = heading_diff_l < 0 ? -heading_diff_l : heading_diff_l;

	if (heading_diff_l > 180) {
		if (self->heading < target_heading_g) {
			turn = -5;

		} else if (self->heading > target_heading_g) {
			turn = 5;
		}

	} else if (heading_diff_l > 10) {
		if (self->heading < target_heading_g) {
			turn = 5;

		} else if (self->heading > target_heading_g) {
			turn = -5;
		}

	} else {
		turn = 0;
	}

	angle_n = self->heading / 360.0 * g_pi;
	int32_t step_h_g = (int32_t) (cos(angle_n) * speed_k);
	int32_t step_v_h = (int32_t) (sin(angle_n) * speed_k);
	self->velocity = 0.0;
	self->drift_v = self->velocity;
	self->drift_h = self->drift_v;
	self->phys_tick = 0;

	if (!FordWagonCheckCollision(self))
		return;

	if (self->phys_tick) {
		step_h_g += (int32_t) (self->drift_h / self->phys_tick * self->drift_gain);
		step_v_h += -(int32_t) (self->drift_v / self->phys_tick * self->drift_gain);
	}

	self->velocity = self->velocity / 20.0;

	if ((self->velocity < 0.0 ? -self->velocity : self->velocity) < 10.0)
		self->velocity = 0.0;

	self->heading += (int32_t) self->velocity;
	self->heading += turn;
	self->heading = (self->heading + 360) % 360;
	FORD_WAGON_OPS(self)->SetHeading(self, self->heading);

	if (FordWagonReachedBank(self)) {
		RAFT_FRAME_OPS(self->parent)->Finish(self->parent, self->damage);
		*stop = true;
		return;
	}

	if (FordWagonCanMove(self, step_h_g, step_v_h)) {
		SPR_OBJ_OPS(obj)->MoveBy(obj, step_h_g, step_v_h);
		return;
	}

	int32_t try_h_a = step_h_g;
	int32_t try_v_a;
	int32_t attempt_c;

	for (attempt_c = 0; attempt_c < 30; attempt_c++, try_h_a--) {
		if (FordWagonCanMove(self, try_h_a, step_v_h)) {
			SPR_OBJ_OPS(obj)->MoveBy(obj, try_h_a, step_v_h);
			return;
		}
	}

	try_h_a = step_h_g;

	for (attempt_c = 0; attempt_c < 30; attempt_c++, try_h_a++) {
		if (FordWagonCanMove(self, try_h_a, step_v_h)) {
			SPR_OBJ_OPS(obj)->MoveBy(obj, try_h_a, step_v_h);
			return;
		}
	}

	try_h_a = step_h_g;
	try_v_a = step_v_h;

	for (attempt_c = 0; attempt_c < 30; attempt_c++, try_h_a--, try_v_a++) {
		if (FordWagonCanMove(self, try_h_a, try_v_a)) {
			SPR_OBJ_OPS(obj)->MoveBy(obj, try_h_a, try_v_a);
			return;
		}
	}

	try_h_a = step_h_g;
	try_v_a = step_v_h;

	for (attempt_c = 0; attempt_c < 30; attempt_c++, try_h_a++, try_v_a++) {
		if (FordWagonCanMove(self, try_h_a, try_v_a)) {
			SPR_OBJ_OPS(obj)->MoveBy(obj, try_h_a, try_v_a);
			return;
		}
	}
}

void ford_wagon_tick(struct spr_obj *obj)
{
	struct ford_wagon *self = (struct ford_wagon *) obj;
	int32_t speed_k = 8;
	int32_t elapsed_g = GameTicks() - g_ford_last_tick;

	if (elapsed_g < 4)
		return;

	g_ford_last_tick = GameTicks();

	switch (RAFT_FRAME_OPS(self->parent)->GetProgress(self->parent)) {
		case 0:
			speed_k = 8;
			break;
		case 1:
			speed_k = 6;
			break;
		case 2:
			speed_k = 5;
			break;
		case 3:
			speed_k = 3;
			break;
	}

	switch (self->anim_count) {
		case 0: {
			bool stop = false;
			steer(self, speed_k, &stop);

			if (stop)
				return;

			break;
		}
		case 5:
		case 6:
		case 7:
			if (GameTicks() > self->anim_next) {
				if (self->anim_frame == self->anim_target) {
					self->anim_count = 1;
					self->anim_next = GameTicks() + 20;

				} else {
					ANIM_SPRITE_OPS(self)->PositionFrame(&self->base, self->anim_target);
					self->anim_target += self->anim_step;
					self->anim_next = GameTicks() + self->anim_delay;
				}
			}

			break;
		case 1:
			if (GameTicks() > self->anim_next) {
				self->damage += RandRangeAvg(10, 40, 2);

				if (self->damage >= 100) {
					RAFT_FRAME_OPS(self->parent)->Finish(self->parent, 100);

				} else {
					ANIM_SPRITE_OPS(self)->PositionFrame(&self->base, 0);
					int32_t spot_h_g = obj->region1.p2.h;
					int32_t spot_v_p = obj->region1.p2.v;
					struct tile_map *map = root_map(self);
					TILE_MAP_OPS(map)->ScrollTo(map, 0, 0);
					spot_h_g = obj->region1.p2.h;
					spot_v_p = obj->region1.p2.v;
					RAFT_FRAME_OPS(self->parent)->FindStartCell(self->parent, &spot_h_g, &spot_v_p);
					SPR_OBJ_OPS(obj)->SetAnchor(obj, spot_h_g, spot_v_p);
					scroll_to_raft(self, obj->region1.p2);
					self->anim_count = 0;
				}
			}

			break;
		default:
			break;
	}

	anim_sprite_tick(obj);
}

void FordWagonSetTurnAnim(struct ford_wagon *self, int32_t *heading)
{
	OtiiAppPlaySfx((struct otii_app *) g_app, 26405, false);

	int32_t set;

	if (!((*heading < 90 || *heading > 135) && (*heading <= 270 || *heading > 315))) {
		set = FORD_FRAMES_SPIN_A;
		self->anim_count = 5;
		self->anim_frame = self->sprite[FORD_FRAMES_SPIN_A]->base.item_count / 2;
		self->anim_step = 1;
		self->anim_target = 0;

	} else if (!((*heading < 45 || *heading >= 90) && (*heading < 225 || *heading > 270))) {
		set = FORD_FRAMES_SPIN_A;
		self->anim_count = 6;
		self->anim_frame = self->sprite[FORD_FRAMES_SPIN_A]->base.item_count;
		self->anim_step = 1;
		self->anim_target = self->sprite[FORD_FRAMES_SPIN_A]->base.item_count / 2;

	} else if (!(*heading <= 135 || *heading >= 225)) {
		set = FORD_FRAMES_SPIN_B;
		self->anim_count = 5;
		self->anim_frame = self->sprite[FORD_FRAMES_SPIN_B]->base.item_count / 2;
		self->anim_step = 1;
		self->anim_target = 0;

	} else {
		set = FORD_FRAMES_SPIN_B;
		self->anim_count = 5;
		self->anim_frame = self->sprite[FORD_FRAMES_SPIN_B]->base.item_count;
		self->anim_step = 1;
		self->anim_target = self->sprite[FORD_FRAMES_SPIN_B]->base.item_count / 2;
	}

	self->anim_delay = 5;
	self->anim_next = GameTicks() + self->anim_delay;
	spr(self)->image = self->sprite[set];
	self->base.anchor_handle = self->acel[set];

	ANIM_SPRITE_OPS(self)->PositionFrame(&self->base, self->anim_target);
}

bool FordWagonCanMove(struct ford_wagon *self, int32_t dx, int32_t dy)
{
	struct rect moved_rect_d = spr(self)->region1.rect;
	struct tile_map *map;

	moved_rect_d.left += dx;
	moved_rect_d.right += dx;
	moved_rect_d.top += dy;
	moved_rect_d.bottom += dy;
	add_view(&moved_rect_d, root_map(self));

	map = root_map(self);

	if (RectContains(&map->map_rect, &moved_rect_d))
		return false;

	if ((dx < 0 && moved_rect_d.left < 0) || (dy < 0 && moved_rect_d.top < 0) ||
		(dx > 0 && moved_rect_d.right > 16 * (int32_t) self->grid_w) ||
		(dy > 0 && moved_rect_d.bottom > 16 * (int32_t) self->grid_h))
		return false;

	to_cells(&moved_rect_d);
	clip_to_grid(self, &moved_rect_d);

	for (int32_t cell_row_e = moved_rect_d.top; cell_row_e < moved_rect_d.bottom; cell_row_e++) {
		int32_t cell_ptr_g = cell_row_e * (int32_t) self->grid_w + moved_rect_d.left;

		for (int32_t cell_col_g = moved_rect_d.left; cell_col_g < moved_rect_d.right; cell_col_g++) {
			uint32_t tile_o = grid_cell(self, cell_ptr_g++);

			if (tile_o == RAFT_LAND || tile_o >= RAFT_SHORE_FIRST)
				return false;
		}
	}

	return true;
}

bool FordWagonReachedBank(struct ford_wagon *self)
{
	struct rect rect = spr(self)->region1.rect;

	add_view(&rect, root_map(self));

	if (rect.top <= 16) {
		return true;

	} else {
		return false;
	}
}

bool FordWagonCheckCollision(struct ford_wagon *self)
{
	struct spr_obj *obj = spr(self);
	struct rect cell_rect_i = obj->region1.rect;
	bool result_g = true;

	self->offset_v = 0;
	self->offset_h = self->offset_v;
	cell_rect_i.left += self->offset_h;
	cell_rect_i.right += self->offset_h;
	cell_rect_i.top += self->offset_v;
	cell_rect_i.bottom += self->offset_v;
	add_view(&cell_rect_i, root_map(self));

	struct rect pixel_rect_g = cell_rect_i;
	to_cells(&cell_rect_i);
	clip_to_grid(self, &cell_rect_i);

	for (int32_t cell_row_g = cell_rect_i.top; cell_row_g < cell_rect_i.bottom; cell_row_g++) {
		int32_t cell_ptr_j = cell_row_g * (int32_t) self->grid_w + cell_rect_i.left;

		for (int32_t cell_col_j = cell_rect_i.left; cell_col_j < cell_rect_i.right; cell_col_j++) {
			uint32_t tile_c = grid_cell(self, cell_ptr_j++);
			int32_t probe_h_l = 16 * (int32_t) cell_col_j + 8;
			int32_t probe_v_e = 16 * (int32_t) cell_row_g + 8;

			if (SPRITE_OPS(obj->image)->HitTest(obj->image, (uint16_t) (probe_h_l - pixel_rect_g.left),
				(uint16_t) (probe_v_e - pixel_rect_g.top), obj->frame)) {
				struct tile_map *map = root_map(self);
				probe_h_l -= map->scroll_pos.h;
				probe_v_e -= map->scroll_pos.v;

				if (tile_c >= RAFT_CURRENT_FIRST && tile_c <= RAFT_CURRENT_LAST) {
					int32_t current_dir_o = (int32_t) ((tile_c - 2) % 8);
					int32_t strength_a = (int32_t) ((tile_c - 2) / 8 + 1);
					result_g = FORD_WAGON_OPS(self)->PhysicsStep(self, current_dir_o, strength_a, probe_h_l,
						probe_v_e);

				} else if (tile_c > RAFT_CURRENT_LAST && tile_c < RAFT_START) {
					FordWagonSetTurnAnim(self, &self->heading);
					result_g = false;

				} else {
					result_g = FORD_WAGON_OPS(self)->PhysicsStep(self, 0, 1, probe_h_l, probe_v_e);
				}
			}
		}
	}

	return result_g;
}

bool ford_wagon_physics_step(struct ford_wagon *self, int32_t direction, int32_t speed, int32_t target_v,
	int32_t target_h)
{
	const struct raft_frame_ops *parent_ops = RAFT_FRAME_OPS(self->parent);
	double angle_p = (double) g_current_angle[direction];

	if (parent_ops->GetProgress(self->parent) == 2 || parent_ops->GetProgress(self->parent) == 3) {
		int32_t heading_diff_o = self->heading - g_current_angle[direction];

		if (heading_diff_o < 0)
			heading_diff_o = -heading_diff_o;

		if (heading_diff_o > 180)
			heading_diff_o = 360 - heading_diff_o;

		if (heading_diff_o < 100) {
			heading_diff_o = 0;
			self->spin_risk -= 1000;

			if (self->spin_risk < 0)
				self->spin_risk = 0;

		} else {
			heading_diff_o -= 100;
			heading_diff_o /= 5;
		}

		heading_diff_o = heading_diff_o + speed;
		self->spin_risk += heading_diff_o;

		if (self->spin_risk > 1000) {
			heading_diff_o = self->spin_risk / 1000;

			if (RandRangeAvg(0, 9, 1) < heading_diff_o) {
				FordWagonSetTurnAnim(self, &self->heading);
				return false;
			}
		}
	}

	if (angle_p != 0.0)
		angle_p = angle_p / 360.0 * 6.283185307179586;

	double push_h_h = cos(angle_p) * parent_ops->GetDrift(self->parent) * (double) speed * 1000.0;
	double push_v_k = sin(angle_p) * parent_ops->GetDrift(self->parent) * (double) speed * 1000.0;
	push_h_h = push_h_h / 1000.0;
	push_v_k = push_v_k / 1000.0;
	self->drift_h += push_h_h;
	self->drift_v += push_v_k;
	++self->phys_tick;

	if (direction) {
		struct rect bounds_p = spr(self)->region1.rect;
		int32_t center_h_p = bounds_p.left + (bounds_p.right - bounds_p.left) / 2;
		int32_t center_v_m = bounds_p.top + (bounds_p.bottom - bounds_p.top) / 2;
		int32_t offset_h_e = target_v - center_h_p;
		int32_t offset_v_d = target_h - center_v_m;
		double spin_h_b = 0.0;
		double spin_v_e = 0.0;

		if (offset_h_e > 0) {
			spin_h_b = spin_h_b - speed * offset_h_e;

		} else {
			offset_h_e = -offset_h_e;
			spin_h_b = speed * offset_h_e + spin_h_b;
		}

		if (offset_v_d > 0) {
			spin_v_e = spin_v_e - speed * offset_v_d;

		} else {
			offset_v_d = -offset_v_d;
			spin_v_e = speed * offset_v_d + spin_v_e;
		}

		double spin_a = spin_v_e + spin_h_b;
		self->velocity += spin_a;
	}

	return true;
}

void ford_wagon_move_by(struct spr_obj *obj, int32_t dx, int32_t dy)
{
	obj->region1.rect.left += dx;
	obj->region1.rect.right += dx;
	obj->region1.rect.top += dy;
	obj->region1.rect.bottom += dy;
	obj->region1.p2.v += dy;
	obj->region1.p2.h += dx;

	if (dx != 0 || dy != 0)
		obj->dirty = true;
}

void ford_wagon_set_heading(struct ford_wagon *self, int32_t heading)
{
	int32_t frame_count_i = self->sprite[FORD_FRAMES_HEADINGS]->base.item_count;
	int32_t frame_bias_i = frame_count_i + frame_count_i / 2;

	spr(self)->image = self->sprite[FORD_FRAMES_HEADINGS];
	self->base.anchor_handle = self->acel[FORD_FRAMES_HEADINGS];

	int32_t frame_offset_a = (90 - heading) * frame_count_i / 360;
	ANIM_SPRITE_OPS(self)->PositionFrame(&self->base, (frame_offset_a + frame_bias_i) % frame_count_i);
}

struct view_surface *ford_wagon_get_root_view(struct ford_wagon *self)
{
	return spr(self)->owner;
}
