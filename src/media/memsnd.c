#include "memsnd.h"

#include <stddef.h>

#include "../base/memory.h"
#include "../app/replay.h"
#include "strmdsnd.h"

const struct mem_snd_ops MEM_SND_OPS_TABLE = {
	{
		{
			{
				{mem_snd_destroy},
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
			mem_snd_start_playback,
			snd_base_resume_playback,
			snd_base_pause,
			snd_base_resume,
			snd_obj_slot24,
		},
	},
};

void MemSndInit(struct mem_snd *self, struct server *source, uint8_t *mem, int32_t mem_size)
{
	SndBaseInit(&self->base, source);
	self->base.base.base.base.ops = &MEM_SND_OPS_TABLE.base.base.base.base;

	self->base.base.base.size = sizeof(struct mem_snd);
	self->mem = mem;
	self->mem_size = mem_size;

	if (self->mem)
		self->data = self->mem;
}

struct mem_snd *MemSndCreate(struct server *source, uint8_t *mem, int32_t mem_size)
{
	struct mem_snd *self = MemAlloc(1, sizeof(struct mem_snd));
	MemSndInit(self, source, mem, mem_size);

	return self;
}

void mem_snd_destroy(struct object *obj)
{
	struct mem_snd *self = (struct mem_snd *) obj;

	if (self->mem) {
		MemFree(self->mem);
		self->mem = NULL;
		self->data = NULL;
	}

	obj->ops = &SND_BASE_OPS_TABLE.base.base.base;
	snd_base_destroy(obj);
}

int32_t mem_snd_start_playback(struct snd_obj *obj)
{
	struct mem_snd *self = (struct mem_snd *) obj;
	struct snd_base *base = &self->base;

	base->looping = obj->loop;

	if (!self->mem)
		return 3;

	base->handle = RpSoundHandle(StrmdSndPlayMem(self->data, self->mem_size, base->looping));

	if (base->handle < 1)
		return 3;

	StrmdSndVolume(base->handle, obj->level);

	return 1;
}
