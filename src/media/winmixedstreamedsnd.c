#include "winmixedstreamedsnd.h"

#include <stddef.h>

#include "../base/memory.h"
#include "../app/replay.h"
#include "sndstrm.h"
#include "strmdsnd.h"

static int32_t cwin_callback(void *ctx);

const struct win_mixed_streamed_snd_ops WIN_MIXED_STREAMED_SND_OPS_TABLE = {
	{
		{
			{
				{win_mixed_streamed_snd_destroy},
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
			win_mixed_streamed_snd_stop,
			snd_base_play_sync,
			snd_base_set_level,
			snd_obj_slot13,
			snd_base_get_level,
			win_mixed_streamed_snd_set_loop,
			snd_obj_play_cue_range,
			snd_obj_slot17,
			snd_base_get_status,
			win_mixed_streamed_snd_get_position,
			win_mixed_streamed_snd_start_playback,
			snd_base_resume_playback,
			snd_base_pause,
			snd_base_resume,
			snd_obj_slot24,
		},
	},
};

void WinMixedStreamedSndInit(struct win_mixed_streamed_snd *self, struct server *source, struct snd_strm *stream,
	int32_t buf_size)
{
	SndBaseInit(&self->base, source);
	self->base.base.base.base.ops = &WIN_MIXED_STREAMED_SND_OPS_TABLE.base.base.base.base;

	self->base.base.base.size = sizeof(struct win_mixed_streamed_snd);
	self->buf_size = buf_size;
	self->mem = MemAlloc((size_t) self->buf_size, 1);
	self->mem_size = self->buf_size;
	self->play_ptr = NULL;
	self->stream = stream;
}

struct win_mixed_streamed_snd *WinMixedStreamedSndCreate(struct server *source, struct snd_strm *stream,
	int32_t buf_size)
{
	struct win_mixed_streamed_snd *self = MemAlloc(1, sizeof(struct win_mixed_streamed_snd));
	WinMixedStreamedSndInit(self, source, stream, buf_size);

	return self;
}

void win_mixed_streamed_snd_destroy(struct object *obj)
{
	struct win_mixed_streamed_snd *self = (struct win_mixed_streamed_snd *) obj;

	StrmdSndLock();
	ObjectDestroy(&self->stream);
	MemFree(self->mem);
	self->mem = NULL;
	self->play_ptr = NULL;
	StrmdSndUnlock();

	obj->ops = &SND_BASE_OPS_TABLE.base.base.base;
	snd_base_destroy(obj);
}

int32_t win_mixed_streamed_snd_start_playback(struct snd_obj *obj)
{
	struct win_mixed_streamed_snd *self = (struct win_mixed_streamed_snd *) obj;
	struct snd_base *base = &self->base;
	struct stream *stream = &self->stream->base;

	int32_t buf_len = self->mem_size;
	base->looping = obj->loop;
	self->play_ptr = self->mem;
	StrmdSndLock();
	STREAM_OPS(stream)->Seek(stream, 0);
	STREAM_OPS(stream)->Get(stream, self->play_ptr, &buf_len);
	StrmdSndUnlock();
	base->handle = RpSoundHandle(StrmdSndPlayCallback(self->play_ptr, buf_len, cwin_callback, self));

	if (base->handle < 1)
		return 3;

	StrmdSndVolume(base->handle, obj->level);

	return 1;
}

int32_t win_mixed_streamed_snd_stop(struct snd_obj *obj)
{
	struct win_mixed_streamed_snd *self = (struct win_mixed_streamed_snd *) obj;

	snd_base_stop(obj);

	if (self->play_ptr)
		self->play_ptr = NULL;

	return 0;
}

int32_t win_mixed_streamed_snd_set_loop(struct snd_obj *obj, int32_t start, int32_t loop_start, int32_t end,
	int32_t flags)
{
	struct win_mixed_streamed_snd *self = (struct win_mixed_streamed_snd *) obj;

	if (loop_start < 0)
		self->base.looping = false;

	if (!self->base.looping)
		loop_start = -1;

	StrmdSndLock();
	SND_STRM_OPS(self->stream)->SetSelection(self->stream, start, loop_start, end, flags);
	StrmdSndUnlock();

	return 0;
}

int32_t win_mixed_streamed_snd_get_position(struct snd_obj *obj)
{
	struct win_mixed_streamed_snd *self = (struct win_mixed_streamed_snd *) obj;

	return RpQuery(RPQ_SOUND_POS, StrmdSndGetPos(self->base.handle));
}

int32_t WinMixedStreamedSndFillBuffer(struct win_mixed_streamed_snd *self)
{
	if (!self->stream || !self->play_ptr)
		return -1;

	struct stream *stream = &self->stream->base;

	int32_t filled = self->buf_size;
	int32_t status = STREAM_OPS(stream)->Get(stream, self->play_ptr, &filled);

	if (status == 1) {
		if (self->base.looping) {
			STREAM_OPS(stream)->Seek(stream, 0);

			if (!filled) {
				filled = self->buf_size;
				STREAM_OPS(stream)->Get(stream, self->play_ptr, &filled);
			}

		} else {
			filled = -1;
		}
	}

	return filled;
}

static int32_t cwin_callback(void *ctx)
{
	return WinMixedStreamedSndFillBuffer(ctx);
}
