#pragma once

#include <stdint.h>

#include "stream.h"

struct snd_strm;

struct snd_strm_ops {
	struct stream_ops base;
	int32_t (*Channels)(struct snd_strm *self);
	int32_t (*SampleSize)(struct snd_strm *self);
	int32_t (*SampleRate)(struct snd_strm *self);
	int32_t (*FadeTrack)(struct snd_strm *self, int32_t ch, int32_t vol, int32_t ramp);
	int32_t (*SetSelection)(struct snd_strm *self, int32_t start, int32_t loop_start, int32_t end, int32_t flags);
	int32_t (*SyncTo)(struct snd_strm *self, struct snd_strm *master);
};

struct snd_strm {
	struct stream base;
	int32_t loop_start;
};

#define SND_STRM_OPS(p) ((const struct snd_strm_ops *) ((const struct object *) (p))->ops)

extern const struct snd_strm_ops SND_STRM_OPS_TABLE;

void SndStrmInit(struct snd_strm *self);
void snd_strm_destroy(struct object *self);

int32_t snd_strm_channels(struct snd_strm *self);
int32_t snd_strm_sample_size(struct snd_strm *self);
int32_t snd_strm_sample_rate(struct snd_strm *self);
int32_t snd_strm_fade_track(struct snd_strm *self, int32_t ch, int32_t vol, int32_t ramp);
int32_t snd_strm_set_selection(struct snd_strm *self, int32_t start, int32_t loop_start, int32_t end, int32_t flags);
int32_t snd_strm_sync_to(struct snd_strm *self, struct snd_strm *master);

int32_t SndStrmGetLoopStart(struct snd_strm *self);
