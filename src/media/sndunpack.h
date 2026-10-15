#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../data/decode.h"
#include "stream.h"

struct snd_unpack;

struct key_frame {
	int32_t sample_pos;
	int32_t bit_offset;
};

struct snd_unpack_ops {
	struct stream_ops base;
};

struct snd_unpack {
	struct stream base;
	struct stream *source;
	bool header_parsed;
	int32_t total_len;
	int32_t block_count;
	int32_t flags;
	int32_t bit_position;
	struct decode_table mtf_table;
	int32_t repeat_count;
	uint8_t repeat_sample[4];
	uint8_t block_buf[64];
	uint8_t skip_buf[1024];
	int32_t pending_bytes;
	int32_t tell_base;
	int32_t data_offset;
	int32_t reserved_578;
	int32_t tell_offset;
	int32_t end_bit_offset;
	int32_t keyframe_count;
	struct key_frame *keyframes;
};

#define SND_UNPACK_OPS(p) ((const struct snd_unpack_ops *) ((const struct object *) (p))->ops)

extern const struct snd_unpack_ops SND_UNPACK_OPS_TABLE;

void SndUnpackInit(struct snd_unpack *self, struct stream *source);
struct snd_unpack *SndUnpackCreate(struct stream *source);
void snd_unpack_destroy(struct object *self);

bool snd_unpack_free(struct stream *self);
bool snd_unpack_unfree(struct stream *self);
bool snd_unpack_purged(struct stream *self);
struct stream *snd_unpack_clone(struct stream *self);
int32_t snd_unpack_get(struct stream *self, uint8_t *buf, int32_t *len);
int32_t snd_unpack_seek(struct stream *self, int32_t pos);
int32_t snd_unpack_position(struct stream *self);
int32_t snd_unpack_size(struct stream *self);
int32_t snd_unpack_set_start(struct stream *self, int32_t pos);
int32_t snd_unpack_set_end(struct stream *self, int32_t pos);
int32_t snd_unpack_status(struct stream *self);
