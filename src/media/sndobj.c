#include "sndobj.h"

#include <stddef.h>

#include "../base/byteorder.h"
#include "../data/rsrc.h"
#include "sndstream.h"

const struct snd_obj_ops SND_OBJ_OPS_TABLE = {
	{
		{snd_obj_destroy},
		media_base_free,
		media_base_release_lock,
		media_base_is_unlocked,
		media_base_clone,
		media_base_release_shared,
		snd_obj_release_source,
	},
	snd_obj_begin,
	snd_obj_finish,
	snd_obj_start,
	snd_obj_stop,
	snd_obj_play_sync,
	snd_obj_set_level,
	snd_obj_slot13,
	snd_obj_get_level,
	snd_obj_set_loop,
	snd_obj_play_cue_range,
	snd_obj_slot17,
	snd_obj_get_status,
	snd_obj_get_position,
	snd_obj_start_playback,
	snd_obj_resume_playback,
	snd_obj_pause,
	snd_obj_resume,
	snd_obj_slot24,
};

void SndObjInit(struct snd_obj *self, struct server *arg)
{
	MediaBaseInit(&self->base, arg);
	self->base.base.ops = &SND_OBJ_OPS_TABLE.base.base;

	self->base.size = sizeof(struct snd_obj);
	self->playing = false;
	self->level = 0xFF;
	self->cue_points = NULL;
	self->status = 0;
}

void snd_obj_destroy(struct object *obj)
{
	obj->ops = &MEDIA_BASE_OPS_TABLE.base;
	media_base_destroy(obj);
}

void snd_obj_release_source(struct media_base *obj)
{
	struct snd_obj *self = (struct snd_obj *) obj;

	if (self->cue_points)
		RsrcDestroy(&self->cue_points);

	SND_OBJ_OPS(self)->Stop(self);
}

int32_t snd_obj_begin(struct snd_obj *self, bool loop)
{
	if (self->status == 4)
		return 3;

	SND_OBJ_OPS(self)->Stop(self);
	self->playing = false;
	self->loop = loop;

	if (!self->base.server) {
		return SND_OBJ_OPS(self)->StartPlayback(self);

	} else {
		if (((struct snd_stream *) self->base.server)->enabled) {
			return SND_OBJ_OPS(self)->StartPlayback(self);

		} else {
			return 0;
		}
	}
}

int32_t snd_obj_start_playback(struct snd_obj *self)
{
	(void) self;

	return 0;
}

int32_t snd_obj_start(struct snd_obj *self)
{
	int32_t res = 0;

	if (SND_OBJ_OPS(self)->GetStatus(self) == 1) {
		res = SND_OBJ_OPS(self)->Stop(self);
		self->playing = true;
	}

	return res;
}

int32_t snd_obj_finish(struct snd_obj *self)
{
	int32_t res;

	if (self->playing) {
		if (!self->base.server) {
			res = SND_OBJ_OPS(self)->ResumePlayback(self);

		} else {
			if (((struct snd_stream *) self->base.server)->enabled) {
				res = SND_OBJ_OPS(self)->ResumePlayback(self);

			} else {
				res = 0;
			}
		}

		self->playing = false;

		return res;

	} else {
		return 0;
	}
}

int32_t snd_obj_resume_playback(struct snd_obj *self)
{
	return SND_OBJ_OPS(self)->StartPlayback(self);
}

int32_t snd_obj_stop(struct snd_obj *self)
{
	self->playing = false;

	return 0;
}

int32_t snd_obj_play_sync(struct snd_obj *self)
{
	int32_t res = SND_OBJ_OPS(self)->Begin(self, false);

	if (res != 1)
		return res;

	while (SND_OBJ_OPS(self)->GetStatus(self) == 1)
		;

	return 0;
}

int32_t snd_obj_get_status(struct snd_obj *self)
{
	return self->status;
}

int32_t snd_obj_set_level(struct snd_obj *self, int32_t level)
{
	self->level = level;

	return self->status;
}

int32_t snd_obj_slot13(struct snd_obj *self, int32_t a, int32_t b, int32_t c)
{
	(void) self;
	(void) a;
	(void) b;
	(void) c;

	return 0;
}

int32_t snd_obj_slot17(struct snd_obj *self, int32_t a)
{
	(void) self;
	(void) a;

	return 0;
}

int32_t snd_obj_slot24(struct snd_obj *self)
{
	(void) self;

	return -1;
}

int32_t snd_obj_pause(struct snd_obj *self)
{
	return SND_OBJ_OPS(self)->Start(self);
}

int32_t snd_obj_resume(struct snd_obj *self)
{
	return SND_OBJ_OPS(self)->Finish(self);
}

int32_t snd_obj_get_level(struct snd_obj *self)
{
	return self->level;
}

int32_t snd_obj_set_loop(struct snd_obj *self, int32_t start, int32_t b, int32_t c, int32_t d)
{
	(void) self;
	(void) start;
	(void) b;
	(void) c;
	(void) d;

	return 0;
}

int32_t snd_obj_play_cue_range(struct snd_obj *self, int32_t start_cue, int32_t loop_cue, int32_t end_cue,
	int32_t flags)
{
	if (!self->cue_points)
		return 3;

	int32_t cue_count = (int16_t) ((uint32_t) RsrcSize(self->cue_points) >> 3);

	if (!cue_count)
		return 3;

	int32_t start_offset = -1;
	int32_t loop_offset = -1;
	int32_t end_offset = -1;
	const uint8_t *cues = RsrcData(self->cue_points);

	for (int32_t index = 0; cue_count > index; ++index) {
		int32_t cue_id = ReadLe32(cues + 4);

		if (cue_id == start_cue)
			start_offset = ReadLe32(cues);

		if (cue_id == loop_cue)
			loop_offset = ReadLe32(cues);

		if (cue_id == end_cue)
			end_offset = ReadLe32(cues);

		cues += 8;
	}

	if (start_offset == -1 || end_offset == -1)
		return 3;

	SND_OBJ_OPS(self)->SetLoop(self, start_offset, loop_offset, end_offset, flags);

	return 0;
}

void SndObjSetSource(struct snd_obj *self, struct rsrc *src)
{
	if (self->cue_points)
		RsrcDestroy(&self->cue_points);

	self->cue_points = src;
}

int32_t snd_obj_get_position(struct snd_obj *self)
{
	(void) self;

	return 0;
}
