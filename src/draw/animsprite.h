#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "sprobj.h"

struct anim_sprite;
struct assets;
struct rsrc;
struct sprite;
struct view_surface;

struct anim_sprite_ops {
	struct spr_obj_ops base;
	void (*PositionFrame)(struct anim_sprite *self, int32_t frame);
	void (*PlaySequence)(struct anim_sprite *self, int32_t frame, int32_t frame_duration);
	void (*SetSequence)(struct anim_sprite *self, struct rsrc *seq, struct rsrc *pos, bool owns);
	int32_t (*GetCurrentSequence)(struct anim_sprite *self);
	void (*GetFrameAnchor)(struct anim_sprite *self, int32_t frame_index, struct point *out);
};

struct anim_sprite {
	struct spr_obj base;
	int32_t cur_seq;
	int32_t step_duration;
	int32_t cur_step;
	int32_t next_seq;
	int32_t steps_offset;
	int32_t steps_left;
	int32_t next_tick_time;
	bool owns_resources;
	struct rsrc *seq_handle;
	struct rsrc *anchor_handle;
};

#define ANIM_SPRITE_OPS(p) ((const struct anim_sprite_ops *) ((const struct object *) (p))->ops)

extern const struct anim_sprite_ops ANIM_SPRITE_OPS_TABLE;

void AnimSpriteInit(struct anim_sprite *self);
void AnimSpriteInitSeq(struct anim_sprite *self, struct view_surface *owner, struct sprite *image, struct rsrc *seq,
	struct rsrc *pos, bool owns);
void AnimSpriteInitLib(struct anim_sprite *self, struct view_surface *owner, struct sprite *image,
	struct assets *provider, int32_t id);
struct anim_sprite *AnimSpriteCreate(void);
struct anim_sprite *AnimSpriteCreateSeq(struct view_surface *owner, struct sprite *image, struct rsrc *seq,
	struct rsrc *pos, bool owns);
struct anim_sprite *AnimSpriteCreateLib(struct view_surface *owner, struct sprite *image, struct assets *provider,
	int32_t id);
void AnimSpriteDestroy(struct anim_sprite **ctx_mod);
void anim_sprite_destroy(struct object *self);

int32_t AnimSpriteAcelCount(const uint8_t *acel);
struct point AnimSpriteAcelAnchor(const uint8_t *acel, int32_t frame);

void AnimSpriteInitMethod(struct anim_sprite *self, struct view_surface *owner, struct sprite *image,
	struct rsrc *seq, struct rsrc *pos, bool owns);

void anim_sprite_set_sequence(struct anim_sprite *self, struct rsrc *seq, struct rsrc *pos, bool owns);
void anim_sprite_play_sequence(struct anim_sprite *self, int32_t frame, int32_t frame_duration);
void anim_sprite_tick(struct spr_obj *self);
void anim_sprite_position_frame(struct anim_sprite *self, int32_t frame);
void anim_sprite_get_frame_anchor(struct anim_sprite *self, int32_t frame_index, struct point *out);
int32_t anim_sprite_get_current_sequence(struct anim_sprite *self);
