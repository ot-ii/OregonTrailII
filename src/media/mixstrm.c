#include "mixstrm.h"

#include <stddef.h>

#include "../base/memory.h"

const struct mix_strm_ops MIX_STRM_OPS_TABLE = {
	{
		{
			{mix_strm_destroy},
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
		snd_strm_set_selection,
		snd_strm_sync_to,
	},
};

void MixStrmInit(struct mix_strm *self)
{
	SndStrmInit(&self->base);
	self->base.base.base.ops = &MIX_STRM_OPS_TABLE.base.base.base;

	MixStrmSetup(self, NULL, 0);
}

void MixStrmInitSources(struct mix_strm *self, struct snd_strm **sources, int32_t channel_count)
{
	SndStrmInit(&self->base);
	self->base.base.base.ops = &MIX_STRM_OPS_TABLE.base.base.base;

	MixStrmSetup(self, sources, channel_count);
}

struct mix_strm *MixStrmCreate(struct snd_strm **sources, int32_t channel_count)
{
	struct mix_strm *self = MemAlloc(1, sizeof(struct mix_strm));
	MixStrmInitSources(self, sources, channel_count);

	return self;
}

void MixStrmSetup(struct mix_strm *self, struct snd_strm **sources, int32_t channel_count)
{
	struct snd_strm *snd = &self->base;
	int32_t index;

	self->channels = NULL;
	self->channel_count = 0;
	self->size = -1;
	self->n_channels = 1;
	self->sample_rate = 22;
	self->sample_size = 8;
	self->position = 0;
	self->out_pos = self->fill_level = 0;

	for (index = 0; index < 0x400; ++index)
		self->mix_buf[index] = 0x80;

	self->channel_count = channel_count;

	if (self->channel_count == 0)
		return;

	self->channels = MemAlloc((size_t) channel_count, sizeof(struct mix_channel));

	for (index = 0; index < channel_count; ++index) {
		self->channels[index].source = sources[index];
		self->channels[index].fill_pos = 0;
	}

	struct snd_strm *first = self->channels[0].source;
	self->n_channels = SND_STRM_OPS(first)->Channels(first);
	self->sample_rate = SND_STRM_OPS(first)->SampleRate(first);
	self->sample_size = SND_STRM_OPS(first)->SampleSize(first);
	int32_t mismatch = 0;

	for (index = 1; index < self->channel_count; ++index) {
		struct snd_strm *source = self->channels[index].source;

		if (SND_STRM_OPS(source)->Channels(source) != self->n_channels)
			mismatch |= 1;

		if (SND_STRM_OPS(source)->SampleRate(source) != self->sample_rate)
			mismatch |= 1;

		if (SND_STRM_OPS(source)->SampleSize(source) != self->sample_size)
			mismatch |= 1;
	}

	if (mismatch != 0) {
		snd->base.error_code = 3;
		return;
	}

	self->size = -1;

	for (index = 1; index < self->channel_count; ++index) {
		struct stream *source = &self->channels[index].source->base;
		int32_t src_size = STREAM_OPS(source)->Size(source);

		if (src_size < 0) {
			self->size = -1;
			break;
		}

		if (self->size < 0) {
			self->size = src_size;

		} else if (src_size > 0 && self->size > src_size) {
			self->size = src_size;
		}
	}

	for (index = 0; index < self->channel_count; ++index) {
		self->channels[index].volume = 0x100;
		self->channels[index].silent = true;
		SND_STRM_OPS(self)->FadeTrack(snd, index, 0x100, 0);
	}

	STREAM_OPS(self)->Seek(&snd->base, 0);
}

void mix_strm_destroy(struct object *obj)
{
	struct mix_strm *self = (struct mix_strm *) obj;

	for (int32_t index = 0; index < self->channel_count; ++index)
		ObjectDelete(self->channels[index].source);

	MemFree(self->channels);

	obj->ops = &SND_STRM_OPS_TABLE.base.base;
	snd_strm_destroy(obj);
}

int32_t mix_strm_get(struct stream *obj, uint8_t *buf, int32_t *len)
{
	struct mix_strm *self = (struct mix_strm *) obj;
	struct mix_channel *channel;
	int32_t chunk;
	int32_t index;

	int32_t result = 1;

	if (self->channel_count == 0)
		return 1;

	struct mix_channel *sync_channel = NULL;

	for (index = 0; index < self->channel_count; ++index) {
		if (self->channels[index].silent) {
			sync_channel = &self->channels[index];
			break;
		}
	}

	for (index = 0; index < self->channel_count; ++index) {
		channel = &self->channels[index];

		if (channel->silent)
			continue;

		if (channel->ramp_len >= 0 && channel->ramp_end_vol != 0) {
			if (sync_channel != NULL)
				SND_STRM_OPS(channel->source)->SyncTo(channel->source, sync_channel->source);

			struct stream *source = &channel->source->base;

			if (STREAM_OPS(source)->Status(source) == 0) {
				channel->silent = true;

			} else {
				channel->silent = false;
			}
		}
	}

	int32_t requested = *len;
	*len = 0;

	while (*len < requested) {
		if (self->fill_level - self->out_pos < requested - *len) {
			self->fill_level = 0x400;

			for (index = 0; index < self->channel_count; ++index) {
				channel = &self->channels[index];
				struct stream *source = &channel->source->base;

				if (source->error_code == 4)
					continue;

				if (!channel->silent)
					continue;

				if (channel->ramp_len >= 0) {
					if (channel->ramp_len == 0 || channel->ramp_len <= channel->ramp_pos) {
						channel->volume = channel->ramp_end_vol;
						channel->ramp_len = -1;

					} else {
						channel->volume = (channel->ramp_end_vol - channel->ramp_start_vol) * channel->ramp_pos /
							channel->ramp_len;
						channel->volume += channel->ramp_start_vol;
					}
				}

				chunk = 0x400 - channel->fill_pos;
				int32_t got = STREAM_OPS(source)->Get(source, self->src_buf, &chunk);

				if (got != 1) {
					result = 0;

				} else {
					result = 1;
				}

				for (int32_t sample = 0; sample < chunk; ++sample) {
					uint8_t scaled = (uint8_t) (((self->src_buf[sample] - 0x80) * channel->volume) >> 8);
					self->mix_buf[channel->fill_pos + sample] =
						(uint8_t) (scaled + self->mix_buf[channel->fill_pos + sample]);
				}

				channel->ramp_pos += chunk;
				channel->fill_pos += chunk;

				if (self->fill_level > channel->fill_pos)
					self->fill_level = channel->fill_pos;
			}
		}

		chunk = requested - *len;

		if (self->fill_level - self->out_pos < chunk)
			chunk = self->fill_level - self->out_pos;

		if (chunk == 0)
			break;

		for (index = 0; index < chunk; ++index) {
			buf[*len] = self->mix_buf[self->out_pos + index];
			++*len;
		}

		self->out_pos += chunk;

		if (self->out_pos >= 0x400) {
			for (index = 0; index < self->channel_count; ++index)
				self->channels[index].fill_pos = 0;

			self->out_pos = 0;
			self->fill_level = 0;

			for (index = 0; index < 0x400; ++index)
				self->mix_buf[index] = 0x80;
		}
	}

	for (index = 0; index < self->channel_count; ++index) {
		channel = &self->channels[index];

		if (channel->volume != 0) {
			channel->silent = true;

		} else {
			channel->silent = false;
		}
	}

	if (self->base.loop_start > 0 && obj->start_pos != self->base.loop_start &&
		obj->start_pos + self->position > self->base.loop_start) {
		self->position -= self->base.loop_start - obj->start_pos;
		STREAM_OPS(obj)->SetStart(obj, self->base.loop_start);
	}

	return result;
}

int32_t mix_strm_size(struct stream *obj)
{
	struct mix_strm *self = (struct mix_strm *) obj;

	return self->size;
}

int32_t mix_strm_position(struct stream *obj)
{
	struct mix_strm *self = (struct mix_strm *) obj;

	return self->position;
}

int32_t mix_strm_seek(struct stream *obj, int32_t pos)
{
	struct mix_strm *self = (struct mix_strm *) obj;
	int32_t result = 0;

	if (self->channel_count == 0)
		return 1;

	for (int32_t index = 0; index < self->channel_count; ++index) {
		struct stream *source = &self->channels[index].source->base;
		self->channels[index].fill_pos = 0;
		result = STREAM_OPS(source)->Seek(source, pos);
	}

	self->out_pos = 0;
	self->fill_level = 0;

	return result;
}

bool mix_strm_free(struct stream *obj)
{
	struct mix_strm *self = (struct mix_strm *) obj;
	bool all_ok = true;

	for (int32_t index = 0; index < self->channel_count; ++index) {
		struct stream *source = &self->channels[index].source->base;

		if (!STREAM_OPS(source)->Free(source))
			all_ok = false;
	}

	return all_ok;
}

bool mix_strm_unfree(struct stream *obj)
{
	struct mix_strm *self = (struct mix_strm *) obj;

	for (int32_t index = 0; index < self->channel_count; ++index) {
		struct stream *source = &self->channels[index].source->base;

		if (!STREAM_OPS(source)->Unfree(source))
			return false;
	}

	return true;
}

bool mix_strm_purged(struct stream *obj)
{
	struct mix_strm *self = (struct mix_strm *) obj;

	for (int32_t index = 0; index < self->channel_count; ++index) {
		struct stream *source = &self->channels[index].source->base;

		if (!STREAM_OPS(source)->Purged(source))
			return false;
	}

	return true;
}

int32_t mix_strm_channels(struct snd_strm *obj)
{
	struct mix_strm *self = (struct mix_strm *) obj;

	return self->n_channels;
}

int32_t mix_strm_sample_size(struct snd_strm *obj)
{
	struct mix_strm *self = (struct mix_strm *) obj;

	return self->sample_size;
}

int32_t mix_strm_sample_rate(struct snd_strm *obj)
{
	struct mix_strm *self = (struct mix_strm *) obj;

	return self->sample_rate;
}

int32_t mix_strm_fade_track(struct snd_strm *obj, int32_t ch, int32_t vol, int32_t ramp)
{
	struct mix_strm *self = (struct mix_strm *) obj;

	if (vol < 0) {
		vol = 0;

	} else if (vol > 0x100) {
		vol = 0x100;
	}

	if (ch < self->channel_count) {
		struct mix_channel *channel = &self->channels[ch];

		if (ramp == 0) {
			channel->ramp_len = -1;
			channel->volume = vol;

			if (vol == 0) {
				channel->silent = true;

			} else {
				channel->silent = false;
			}

		} else {
			channel->ramp_start_vol = channel->volume;
			channel->ramp_end_vol = vol;
			channel->ramp_len = SND_STRM_OPS(self)->SampleRate(obj) * ramp;
			channel->ramp_pos = 0;
		}
	}

	return 0;
}

int32_t mix_strm_status(struct stream *obj)
{
	struct mix_strm *self = (struct mix_strm *) obj;
	int32_t result = 0;

	for (int32_t index = 0; index < self->channel_count; ++index) {
		struct stream *source = &self->channels[index].source->base;
		result = STREAM_OPS(source)->Status(source);

		if (result != 0)
			break;
	}

	return result;
}
