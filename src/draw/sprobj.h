#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "../base/list.h"
#include "../base/object.h"
#include "spriteregion.h"

struct canvas;
struct spr_obj;
struct sprite;
struct view_surface;

struct spr_obj_ops {
	struct object_ops base;
	bool (*RectIntersects)(struct spr_obj *self, struct spr_obj *other);
	void (*DrawMasked)(struct spr_obj *self, struct canvas *target, struct canvas *src_canvas);
	void (*Render)(struct spr_obj *self, struct canvas *target);
	void (*Hide)(struct spr_obj *self);
	void (*Show)(struct spr_obj *self);
	void (*SetImagePos)(struct spr_obj *self, int32_t frame, int32_t h, int32_t v);
	void (*Tick)(struct spr_obj *self);
	void (*MoveBy)(struct spr_obj *self, int32_t dh, int32_t dv);
	void (*SetAnchor)(struct spr_obj *self, int32_t h_g, int32_t v_h);
	bool (*ObjectHitTest)(struct spr_obj *self, struct point *pt);
};

struct spr_obj {
	struct object base;
	struct list_node node;
	struct sprite_region region1;
	int32_t visible;
	int32_t frame;
	bool flip_x;
	int32_t mask_threshold;
	struct sprite_region region2;
	int32_t prev_visible;
	int32_t prev_frame;
	bool prev_flip_x;
	int32_t prev_mask;
	bool dirty;
	bool needs_redraw;
	int32_t tag;
	struct sprite *image;
	struct view_surface *owner;
	bool detach_pending;
};

#define SPR_OBJ_OPS(p) ((const struct spr_obj_ops *) ((const struct object *) (p))->ops)

extern const struct spr_obj_ops SPR_OBJ_OPS_TABLE;

void SprObjInit(struct spr_obj *self);
void SprObjInitOwner(struct spr_obj *self, struct view_surface *owner, struct sprite *image);
struct spr_obj *SprObjCreate(struct view_surface *owner, struct sprite *image);
void SprObjDestroy(struct spr_obj **ctx_mod);
void spr_obj_destroy(struct object *self);

struct spr_obj *SprObjFromNode(struct list_node *node);

void SprObjInitMethod(struct spr_obj *self, struct view_surface *owner, struct sprite *image);
void SprObjDetach(struct spr_obj *self, bool now);

void spr_obj_render(struct spr_obj *self, struct canvas *target);
void spr_obj_draw_masked(struct spr_obj *self, struct canvas *target, struct canvas *src_canvas);
void spr_obj_hide(struct spr_obj *self);
void spr_obj_show(struct spr_obj *self);
void spr_obj_move_by(struct spr_obj *self, int32_t dh, int32_t dv);
void spr_obj_set_anchor(struct spr_obj *self, int32_t h_g, int32_t v_h);
void spr_obj_set_image_pos(struct spr_obj *self, int32_t frame, int32_t h, int32_t v);
void spr_obj_tick(struct spr_obj *self);
bool spr_obj_object_hit_test(struct spr_obj *self, struct point *pt);
bool spr_obj_rect_intersects(struct spr_obj *self, struct spr_obj *other);
