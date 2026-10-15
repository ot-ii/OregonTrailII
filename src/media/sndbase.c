#include "sndbase.h"

#include <stddef.h>

#include "../app/replay.h"
#include "strmdsnd.h"

const struct snd_base_ops SND_BASE_OPS_TABLE = {
	{
		{
			{snd_base_destroy},
			media_base_free,
			media_base_release_lock,
			media_base_is_unlocked,
			media_base_clone,
			media_base_release_shared,
			snd_obj_release_source,
		},
		snd_obj_begin,
		snd_obj_finish,
		snd_base_start,
		snd_base_stop,
		snd_base_play_sync,
		snd_base_set_level,
		snd_obj_slot13,
		snd_base_get_level,
		snd_obj_set_loop,
		snd_obj_play_cue_range,
		snd_obj_slot17,
		snd_base_get_status,
		snd_obj_get_position,
		snd_base_start_playback,
		snd_base_resume_playback,
		snd_base_pause,
		snd_base_resume,
		snd_obj_slot24,
	},
};

void SndBaseInit(struct snd_base *self, struct server *source)
{
	SndObjInit(&self->base, source);
	self->base.base.base.ops = &SND_BASE_OPS_TABLE.base.base.base;

	self->base.base.size = sizeof(struct snd_base);
	self->handle = 0;
}

void snd_base_destroy(struct object *obj)
{
	snd_base_stop((struct snd_obj *) obj);

	obj->ops = &SND_OBJ_OPS_TABLE.base.base;
	snd_obj_destroy(obj);
}

int32_t snd_base_pause(struct snd_obj *self)
{
	(void) self;

	return 0;
}

int32_t snd_base_resume(struct snd_obj *self)
{
	(void) self;

	return 0;
}

int32_t snd_base_start_playback(struct snd_obj *self)
{
	(void) self;

	return 0;
}

int32_t snd_base_start(struct snd_obj *obj)
{
	struct snd_base *self = (struct snd_base *) obj;

	StrmdSndPause(self->handle);
	obj->playing = true;

	return 0;
}

int32_t snd_base_resume_playback(struct snd_obj *obj)
{
	struct snd_base *self = (struct snd_base *) obj;

	StrmdSndResume(self->handle);

	return 0;
}

int32_t snd_base_stop(struct snd_obj *obj)
{
	struct snd_base *self = (struct snd_base *) obj;

	if (RpQuery(RPQ_SOUND_STATUS, StrmdSndStatus(self->handle)))
		StrmdSndStop(self->handle);

	return 0;
}

int32_t snd_base_play_sync(struct snd_obj *self)
{
	(void) self;

	return 3;
}

int32_t snd_base_get_status(struct snd_obj *obj)
{
	struct snd_base *self = (struct snd_base *) obj;

	switch (RpQuery(RPQ_SOUND_STATUS, StrmdSndStatus(self->handle))) {
		case 1:
			return 1;
		case 2:
			return 2;
	}

	return 0;
}

int32_t snd_base_get_level(struct snd_obj *self)
{
	(void) self;

	return (int16_t) RpQuery(RPQ_SOUND_AMPLITUDE, StrmdSndAmplitude());
}

int32_t snd_base_set_level(struct snd_obj *obj, int32_t volume)
{
	struct snd_base *self = (struct snd_base *) obj;

	if (RpQuery(RPQ_SOUND_STATUS, StrmdSndStatus(self->handle)) != 1)
		return 0;

	obj->level = volume;

	if (self->handle)
		StrmdSndVolume(self->handle, obj->level);

	return 0;
}
