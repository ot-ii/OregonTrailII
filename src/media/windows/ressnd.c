#include "../ressnd.h"

#include <stddef.h>

#include <windows.h>

#include "../../app/app.h"
#include "../../base/memory.h"
#include "../../app/replay.h"
#include "../../base/str.h"
#include "../strmdsnd.h"

const struct res_snd_ops RES_SND_OPS_TABLE = {
	{
		{
			{
				{res_snd_destroy},
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
			res_snd_stop,
			snd_base_play_sync,
			snd_base_set_level,
			snd_obj_slot13,
			snd_base_get_level,
			snd_obj_set_loop,
			snd_obj_play_cue_range,
			snd_obj_slot17,
			snd_base_get_status,
			snd_obj_get_position,
			res_snd_start_playback,
			snd_base_resume_playback,
			snd_base_pause,
			snd_base_resume,
			snd_obj_slot24,
		},
	},
};

void ResSndInit(struct res_snd *self, struct server *source, int32_t res_id)
{
	char name[12];

	SndBaseInit(&self->base, source);
	self->base.base.base.base.ops = &RES_SND_OPS_TABLE.base.base.base.base;

	self->base.base.base.size = sizeof(struct res_snd);
	StrPrintf(name, "#%u", (uint32_t) res_id);
	HRSRC info = FindResourceA((HINSTANCE) g_hinstance, name, "WAVE");
	self->res_handle = LoadResource((HINSTANCE) g_hinstance, info);
}

struct res_snd *ResSndCreate(struct server *source, int32_t res_id)
{
	struct res_snd *self = MemAlloc(1, sizeof(struct res_snd));
	ResSndInit(self, source, res_id);

	return self;
}

void res_snd_destroy(struct object *obj)
{
	struct res_snd *self = (struct res_snd *) obj;

	if (self->res_handle)
		FreeResource((HGLOBAL) self->res_handle);

	obj->ops = &SND_BASE_OPS_TABLE.base.base.base;
	snd_base_destroy(obj);
}

int32_t res_snd_start_playback(struct snd_obj *obj)
{
	struct res_snd *self = (struct res_snd *) obj;
	struct snd_base *base = &self->base;

	base->looping = obj->loop;
	void *data = LockResource((HGLOBAL) self->res_handle);

	if (!data)
		return 3;

	base->handle = RpSoundHandle(StrmdSndPlayMem(data, (int32_t) GlobalSize((HGLOBAL) self->res_handle),
		base->looping));

	if (base->handle < 1)
		return 3;

	StrmdSndVolume(base->handle, obj->level);

	return 1;
}

int32_t res_snd_stop(struct snd_obj *obj)
{
	snd_base_stop(obj);

	return 0;
}
