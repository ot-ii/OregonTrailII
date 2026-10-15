#include "xfadestrm.h"

#include <stddef.h>

#include "../base/memory.h"

const struct xfade_strm_ops XFADE_STRM_OPS_TABLE = {
	{
		{
			{
				{xfade_strm_destroy},
				mix_strm_free,
				mix_strm_unfree,
				mix_strm_purged,
				stream_clone,
				mix_strm_get,
				stream_get_buf,
				mix_strm_seek,
				stream_seek_ahead,
				mix_strm_position,
				mix_strm_size,
				stream_set_start,
				stream_set_end,
				mix_strm_status,
			},
			mix_strm_channels,
			mix_strm_sample_size,
			mix_strm_sample_rate,
			mix_strm_fade_track,
			xfade_strm_set_selection,
			snd_strm_sync_to,
		},
	},
};

void XfadeStrmInit(struct xfade_strm *self, struct snd_strm *source)
{
	struct stream *s = &self->base.base.base;

	MixStrmInit(&self->base);
	s->base.ops = &XFADE_STRM_OPS_TABLE.base.base.base.base;

	struct stream *clone = STREAM_OPS(source)->Clone(&source->base);

	if (clone == NULL) {
		s->error_code = 4;
		return;
	}

	struct snd_strm *sources[2] = {source, (struct snd_strm *) clone};
	MixStrmSetup(&self->base, sources, 2);

	SND_STRM_OPS(self)->FadeTrack(&self->base.base, 1, 0, 0);
	self->active_channel = 0;
}

struct xfade_strm *XfadeStrmCreate(struct snd_strm *source)
{
	struct xfade_strm *self = MemAlloc(1, sizeof(struct xfade_strm));
	XfadeStrmInit(self, source);

	return self;
}

void xfade_strm_destroy(struct object *obj)
{
	obj->ops = &MIX_STRM_OPS_TABLE.base.base.base;
	mix_strm_destroy(obj);
}

int32_t xfade_strm_set_selection(struct snd_strm *obj, int32_t start, int32_t loop_start, int32_t end, int32_t ramp)
{
	struct xfade_strm *self = (struct xfade_strm *) obj;
	struct mix_channel *channels = self->base.channels;
	struct snd_strm *other = channels[self->active_channel == 0].source;

	SND_STRM_OPS(other)->SetSelection(other, start, loop_start, end, 0);
	SND_STRM_OPS(other)->SyncTo(other, channels[self->active_channel].source);
	SND_STRM_OPS(self)->FadeTrack(obj, self->active_channel, 0, ramp);

	if (self->active_channel == 0) {
		self->active_channel = 1;

	} else {
		self->active_channel = 0;
	}

	SND_STRM_OPS(self)->FadeTrack(obj, self->active_channel, 256, ramp);

	return 0;
}
