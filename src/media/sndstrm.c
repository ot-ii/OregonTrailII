#include "sndstrm.h"

#include <stddef.h>

const struct snd_strm_ops SND_STRM_OPS_TABLE = {
	{
		{snd_strm_destroy},
		stream_free,
		stream_unfree,
		stream_purged,
		stream_clone,
		stream_get,
		stream_get_buf,
		stream_seek,
		stream_seek_ahead,
		stream_position,
		stream_size,
		stream_set_start,
		stream_set_end,
		stream_status,
	},
	snd_strm_channels,
	snd_strm_sample_size,
	snd_strm_sample_rate,
	snd_strm_fade_track,
	snd_strm_set_selection,
	snd_strm_sync_to,
};

void SndStrmInit(struct snd_strm *self)
{
	StreamInit(&self->base);
	self->base.base.ops = &SND_STRM_OPS_TABLE.base.base;

	self->loop_start = -1;
}

void snd_strm_destroy(struct object *obj)
{
	obj->ops = &STREAM_OPS_TABLE.base;
	stream_destroy(obj);
}

int32_t snd_strm_channels(struct snd_strm *self)
{
	(void) self;

	return 1;
}

int32_t snd_strm_sample_size(struct snd_strm *self)
{
	(void) self;

	return 8;
}

int32_t snd_strm_sample_rate(struct snd_strm *self)
{
	(void) self;

	return 22;
}

int32_t snd_strm_fade_track(struct snd_strm *self, int32_t ch, int32_t vol, int32_t ramp)
{
	(void) self;
	(void) ch;
	(void) vol;
	(void) ramp;

	return 0;
}

int32_t snd_strm_set_selection(struct snd_strm *self, int32_t start, int32_t loop_start, int32_t end, int32_t flags)
{
	(void) flags;

	struct stream *s = &self->base;

	STREAM_OPS(s)->SetStart(s, start);
	STREAM_OPS(s)->SetEnd(s, end);
	self->loop_start = loop_start;
	STREAM_OPS(s)->Seek(s, 0);

	return 0;
}

int32_t snd_strm_sync_to(struct snd_strm *self, struct snd_strm *other)
{
	struct stream *s = &self->base;
	struct stream *o = &other->base;

	if (o->start_pos == other->loop_start)
		STREAM_OPS(s)->SetStart(s, self->loop_start);

	return STREAM_OPS(s)->Seek(s, STREAM_OPS(o)->Position(o));
}

int32_t SndStrmGetLoopStart(struct snd_strm *self)
{
	return self->loop_start;
}
