#include "animsprite.h"

#include <stddef.h>

#include "../data/assets.h"
#include "../base/byteorder.h"
#include "../base/memory.h"
#include "../app/replay.h"
#include "../base/rng.h"
#include "../data/rsrc.h"
#include "../media/sndstream.h"
#include "sprite.h"
#include "../media/strmdsnd.h"

const struct anim_sprite_ops ANIM_SPRITE_OPS_TABLE = {
	{
		{anim_sprite_destroy},
		spr_obj_rect_intersects,
		spr_obj_draw_masked,
		spr_obj_render,
		spr_obj_hide,
		spr_obj_show,
		spr_obj_set_image_pos,
		anim_sprite_tick,
		spr_obj_move_by,
		spr_obj_set_anchor,
		spr_obj_object_hit_test,
	},
	anim_sprite_position_frame,
	anim_sprite_play_sequence,
	anim_sprite_set_sequence,
	anim_sprite_get_current_sequence,
	anim_sprite_get_frame_anchor,
};

void AnimSpriteInit(struct anim_sprite *self)
{
	SprObjInit(&self->base);
	self->base.base.ops = &ANIM_SPRITE_OPS_TABLE.base.base;

	self->owns_resources = false;
	self->cur_seq = 255;
	self->next_tick_time = GameTicks();
}

void AnimSpriteInitMethod(struct anim_sprite *self, struct view_surface *owner, struct sprite *image,
	struct rsrc *seq, struct rsrc *pos, bool owns)
{
	SprObjInitMethod(&self->base, owner, image);
	ANIM_SPRITE_OPS(self)->SetSequence(self, seq, pos, owns);
	self->cur_seq = 255;
	self->next_tick_time = GameTicks();
}

void AnimSpriteInitSeq(struct anim_sprite *self, struct view_surface *owner, struct sprite *image, struct rsrc *seq,
	struct rsrc *pos, bool owns)
{
	SprObjInitOwner(&self->base, owner, image);
	self->base.base.ops = &ANIM_SPRITE_OPS_TABLE.base.base;

	self->owns_resources = false;
	ANIM_SPRITE_OPS(self)->SetSequence(self, seq, pos, owns);
	self->cur_seq = 255;
	self->next_tick_time = GameTicks();
}

void AnimSpriteInitLib(struct anim_sprite *self, struct view_surface *owner, struct sprite *image,
	struct assets *provider, int32_t id)
{
	SprObjInitOwner(&self->base, owner, image);
	self->base.base.ops = &ANIM_SPRITE_OPS_TABLE.base.base;

	struct rsrc *seq = AssetsGetElement(provider, "ASeq", id, true);
	RsrcLoadAndUnpack(seq);
	struct rsrc *pos = AssetsGetElement(provider, "ACel", id, true);
	RsrcLoadAndUnpack(pos);
	self->owns_resources = false;
	ANIM_SPRITE_OPS(self)->SetSequence(self, seq, pos, true);
	self->cur_seq = 255;
	self->next_tick_time = GameTicks();
}

struct anim_sprite *AnimSpriteCreate(void)
{
	struct anim_sprite *self = MemAlloc(1, sizeof(struct anim_sprite));
	AnimSpriteInit(self);

	return self;
}

struct anim_sprite *AnimSpriteCreateSeq(struct view_surface *owner, struct sprite *image, struct rsrc *seq,
	struct rsrc *pos, bool owns)
{
	struct anim_sprite *self = MemAlloc(1, sizeof(struct anim_sprite));
	AnimSpriteInitSeq(self, owner, image, seq, pos, owns);

	return self;
}

struct anim_sprite *AnimSpriteCreateLib(struct view_surface *owner, struct sprite *image, struct assets *provider,
	int32_t id)
{
	struct anim_sprite *self = MemAlloc(1, sizeof(struct anim_sprite));
	AnimSpriteInitLib(self, owner, image, provider, id);

	return self;
}

void AnimSpriteDestroy(struct anim_sprite **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void anim_sprite_destroy(struct object *obj)
{
	struct anim_sprite *self = (struct anim_sprite *) obj;

	if (self->owns_resources) {
		if (self->seq_handle)
			RsrcDestroy(&self->seq_handle);

		if (self->anchor_handle)
			RsrcDestroy(&self->anchor_handle);
	}

	obj->ops = &SPR_OBJ_OPS_TABLE.base;
	spr_obj_destroy(obj);
}

void anim_sprite_set_sequence(struct anim_sprite *self, struct rsrc *seq, struct rsrc *pos, bool owns)
{
	if (self->owns_resources) {
		if (self->seq_handle)
			RsrcDestroy(&self->seq_handle);

		if (self->anchor_handle)
			RsrcDestroy(&self->anchor_handle);
	}

	self->seq_handle = seq;
	self->anchor_handle = pos;
	self->owns_resources = owns;
}

struct aseq_sequence {
	int32_t first_step;
	int32_t next_seq;
	int32_t step_duration;
	int32_t step_count;
};

struct aseq_step {
	int32_t picture;
	int32_t dh;
	int32_t dv;
	int32_t sound;
	int32_t duration;
	int32_t flip;
};

static int32_t aseq_count(const uint8_t *data)
{
	return ReadLe16(data);
}

static void aseq_sequence_read(const uint8_t *data, int32_t index, struct aseq_sequence *out)
{
	const uint8_t *p = data + 2 + 8 * index;

	out->first_step = ReadLe16(p);
	out->next_seq = ReadLe16(p + 2);
	out->step_duration = ReadLe16(p + 4);
	out->step_count = ReadLe16(p + 6);
}

static void aseq_step_read(const uint8_t *steps, int32_t index, struct aseq_step *out)
{
	const uint8_t *p = steps + 12 * index + 2;

	out->picture = ReadLe16(p);
	out->dh = ReadLe16(p + 2);
	out->dv = ReadLe16(p + 4);
	out->sound = ReadLe16(p + 6);
	out->duration = ReadLe16(p + 8);
	out->flip = ReadLe16(p + 10);
}

int32_t AnimSpriteAcelCount(const uint8_t *acel)
{
	return ReadLe16(acel);
}

struct point AnimSpriteAcelAnchor(const uint8_t *acel, int32_t frame)
{
	const uint8_t *p = acel + 2 + 4 * frame;

	return PointMake(ReadLe16(p), ReadLe16(p + 2));
}

void anim_sprite_play_sequence(struct anim_sprite *self, int32_t frame, int32_t frame_duration)
{
	if (!self->seq_handle)
		return;

	const uint8_t *seq_data = RsrcData(self->seq_handle);
	int32_t seq_count = aseq_count(seq_data);
	struct aseq_sequence seq;
	aseq_sequence_read(seq_data, frame, &seq);

	self->steps_left = seq.step_count;
	self->cur_step = seq.first_step;
	self->next_seq = seq.next_seq;
	self->cur_seq = frame;

	if (frame_duration < 0) {
		self->step_duration = seq.step_duration;

	} else {
		self->step_duration = frame_duration;
	}

	if (!self->steps_left)
		return;

	self->steps_offset = seq_count * 8 + 2;
	self->next_tick_time = GameTicks();
}

void anim_sprite_tick(struct spr_obj *obj)
{
	struct anim_sprite *self = (struct anim_sprite *) obj;

	if (!self->seq_handle)
		return;

	if (!obj->visible || GameTicks() < self->next_tick_time || self->cur_seq == 255)
		return;

	if (!self->steps_left)
		ANIM_SPRITE_OPS(self)->PlaySequence(self, self->next_seq, -1);

	--self->steps_left;

	struct aseq_step step;
	aseq_step_read(RsrcData(self->seq_handle) + self->steps_offset, self->cur_step, &step);
	++self->cur_step;

	int32_t sound = step.sound;

	if (sound)
		SndStreamStartStream((struct snd_stream *) g_sound_server, SND_KIND_IN_MEMORY, sound, false);

	int32_t dh = step.dh;
	int32_t dv = step.dv;
	int32_t dur = step.duration;

	if (!dur)
		dur = self->step_duration;

	int32_t ticks = GameTicks();
	self->next_tick_time = RandRangeAvg(0, 2, 1) + ticks + dur - 1;

	if (step.flip) {
		if (!obj->flip_x) {
			obj->flip_x = true;

		} else {
			obj->flip_x = false;
		}
	}

	if (obj->flip_x)
		dh *= -1;

	ANIM_SPRITE_OPS(self)->PositionFrame(self, step.picture);

	if (dh || dv)
		SPR_OBJ_OPS(obj)->MoveBy(obj, dh, dv);
}

void anim_sprite_position_frame(struct anim_sprite *self, int32_t frame)
{
	struct point pt = {0};
	struct point anchor = AnimSpriteAcelAnchor(RsrcData(self->anchor_handle), frame);
	int32_t offset_h = anchor.h;
	int32_t offset_v = anchor.v;

	if (self->base.flip_x) {
		struct drawable *image = &self->base.image->base;
		DRAWABLE_OPS(image)->GetSizeAt(image, &pt, self->base.frame);
		offset_h = pt.h - offset_h;
	}

	spr_obj_set_image_pos(&self->base, frame, offset_h, offset_v);
}

void anim_sprite_get_frame_anchor(struct anim_sprite *self, int32_t frame_index, struct point *out)
{
	struct point pt = {0};
	struct point anchor = AnimSpriteAcelAnchor(RsrcData(self->anchor_handle), frame_index);
	int32_t offset_h = anchor.h;
	int32_t offset_v = anchor.v;

	if (self->base.flip_x) {
		struct drawable *image = &self->base.image->base;
		DRAWABLE_OPS(image)->GetSizeAt(image, &pt, self->base.frame);
		offset_h = pt.h - offset_h;
	}

	out->h = offset_h;
	out->v = offset_v;
}

int32_t anim_sprite_get_current_sequence(struct anim_sprite *self)
{
	return self->cur_seq;
}
