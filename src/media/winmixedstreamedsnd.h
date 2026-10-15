#pragma once

#include <stdint.h>

#include "sndbase.h"

struct server;
struct snd_strm;
struct win_mixed_streamed_snd;

struct win_mixed_streamed_snd_ops {
	struct snd_base_ops base;
};

struct win_mixed_streamed_snd {
	struct snd_base base;
	struct snd_strm *stream;
	uint8_t *mem;
	int32_t mem_size;
	int32_t buf_size;
	uint8_t *play_ptr;
};

#define WIN_MIXED_STREAMED_SND_OPS(p) ((const struct win_mixed_streamed_snd_ops *) ((const struct object *) (p))->ops)

extern const struct win_mixed_streamed_snd_ops WIN_MIXED_STREAMED_SND_OPS_TABLE;

void WinMixedStreamedSndInit(struct win_mixed_streamed_snd *self, struct server *source, struct snd_strm *stream,
	int32_t buf_size);
struct win_mixed_streamed_snd *WinMixedStreamedSndCreate(struct server *source, struct snd_strm *stream,
	int32_t buf_size);
void win_mixed_streamed_snd_destroy(struct object *self);

int32_t win_mixed_streamed_snd_stop(struct snd_obj *self);
int32_t win_mixed_streamed_snd_set_loop(struct snd_obj *self, int32_t start, int32_t loop_start, int32_t end,
	int32_t flags);
int32_t win_mixed_streamed_snd_get_position(struct snd_obj *self);
int32_t win_mixed_streamed_snd_start_playback(struct snd_obj *self);

int32_t WinMixedStreamedSndFillBuffer(struct win_mixed_streamed_snd *self);
