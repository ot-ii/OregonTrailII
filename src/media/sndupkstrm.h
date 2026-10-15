#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sndstrm.h"

struct snd_unpack;
struct snd_upk_strm;

struct snd_mark {
	int32_t sample_pos;
	uint8_t delta_state[4];
};

struct snd_upk_strm_ops {
	struct snd_strm_ops base;
};

struct snd_upk_strm {
	struct snd_strm base;
	struct stream *source;
	struct snd_unpack *inner;
	int32_t delta_order;
	int32_t channels;
	int32_t sample_size;
	int32_t rate_code;
	int32_t mark_count;
	struct snd_mark *marks;
	uint8_t delta_state[4];
	int32_t skip_remaining;
	uint8_t sample_buf[1024];
	bool eof_flag;
	int32_t cur_pos;
};

#define SND_UPK_STRM_OPS(p) ((const struct snd_upk_strm_ops *) ((const struct object *) (p))->ops)

extern const struct snd_upk_strm_ops SND_UPK_STRM_OPS_TABLE;

void SndUpkStrmInit(struct snd_upk_strm *self, struct stream *source);
struct snd_upk_strm *SndUpkStrmCreate(struct stream *source);
void snd_upk_strm_destroy(struct object *self);

bool snd_upk_strm_free(struct stream *self);
bool snd_upk_strm_unfree(struct stream *self);
bool snd_upk_strm_purged(struct stream *self);
struct stream *snd_upk_strm_clone(struct stream *self);
int32_t snd_upk_strm_get(struct stream *self, uint8_t *buf, int32_t *len);
int32_t snd_upk_strm_seek(struct stream *self, int32_t pos);
int32_t snd_upk_strm_position(struct stream *self);
int32_t snd_upk_strm_size(struct stream *self);
int32_t snd_upk_strm_set_start(struct stream *self, int32_t start);
int32_t snd_upk_strm_set_end(struct stream *self, int32_t end);
int32_t snd_upk_strm_status(struct stream *self);
int32_t snd_upk_strm_channels(struct snd_strm *self);
int32_t snd_upk_strm_sample_size(struct snd_strm *self);
int32_t snd_upk_strm_sample_rate(struct snd_strm *self);

int32_t SndUpkStrmReadMarks(struct snd_upk_strm *self);
