#include "filesnd.h"

#include <stddef.h>

#include "../base/memory.h"
#include "../app/replay.h"
#include "../base/str.h"
#include "strmdsnd.h"

const struct file_snd_ops FILE_SND_OPS_TABLE = {
	{
		{
			{
				{file_snd_destroy},
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
			file_snd_start_playback,
			snd_base_resume_playback,
			snd_base_pause,
			snd_base_resume,
			snd_obj_slot24,
		},
	},
};

void FileSndInit(struct file_snd *self, struct server *source, int32_t res_id)
{
	SndBaseInit(&self->base, source);
	self->base.base.base.base.ops = &FILE_SND_OPS_TABLE.base.base.base.base;

	self->base.base.base.size = sizeof(struct file_snd);
	self->res_id = res_id;
}

struct file_snd *FileSndCreate(struct server *source, int32_t res_id)
{
	struct file_snd *self = MemAlloc(1, sizeof(struct file_snd));
	FileSndInit(self, source, res_id);

	return self;
}

void file_snd_destroy(struct object *obj)
{
	obj->ops = &SND_BASE_OPS_TABLE.base.base.base;
	snd_base_destroy(obj);
}

int32_t file_snd_start_playback(struct snd_obj *obj)
{
	struct file_snd *self = (struct file_snd *) obj;
	struct snd_base *base = &self->base;
	char path_name[12];

	base->looping = obj->loop;
	StrPrintf(path_name, "%u.wav", (uint32_t) self->res_id);
	base->handle = RpSoundHandle(StrmdSndPlayFile(path_name));

	if (base->handle < 1)
		return 3;

	StrmdSndVolume(base->handle, obj->level);

	return 1;
}
