#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sndstrm.h"

struct mix_strm;

struct mix_channel {
	struct snd_strm *source;
	int32_t volume;
	int32_t fill_pos;
	int32_t ramp_start_vol;
	int32_t ramp_end_vol;
	int32_t ramp_len;
	int32_t ramp_pos;
	bool silent;
};

struct mix_strm_ops {
	struct snd_strm_ops base;
};

struct mix_strm {
	struct snd_strm base;
	struct mix_channel *channels;
	int32_t channel_count;
	int32_t size;
	int32_t n_channels;
	int32_t sample_rate;
	int32_t sample_size;
	int32_t position;
	uint8_t mix_buf[0x400];
	uint8_t src_buf[0x400];
	int32_t out_pos;
	int32_t fill_level;
};

#define MIX_STRM_OPS(p) ((const struct mix_strm_ops *) ((const struct object *) (p))->ops)

extern const struct mix_strm_ops MIX_STRM_OPS_TABLE;

void MixStrmInit(struct mix_strm *self);
void MixStrmInitSources(struct mix_strm *self, struct snd_strm **sources, int32_t channel_count);
struct mix_strm *MixStrmCreate(struct snd_strm **sources, int32_t channel_count);
void mix_strm_destroy(struct object *self);

bool mix_strm_free(struct stream *self);
bool mix_strm_unfree(struct stream *self);
bool mix_strm_purged(struct stream *self);
int32_t mix_strm_get(struct stream *self, uint8_t *buf, int32_t *len);
int32_t mix_strm_seek(struct stream *self, int32_t pos);
int32_t mix_strm_position(struct stream *self);
int32_t mix_strm_size(struct stream *self);
int32_t mix_strm_status(struct stream *self);
int32_t mix_strm_channels(struct snd_strm *self);
int32_t mix_strm_sample_size(struct snd_strm *self);
int32_t mix_strm_sample_rate(struct snd_strm *self);
int32_t mix_strm_fade_track(struct snd_strm *self, int32_t ch, int32_t vol, int32_t ramp);

void MixStrmSetup(struct mix_strm *self, struct snd_strm **sources, int32_t channel_count);
