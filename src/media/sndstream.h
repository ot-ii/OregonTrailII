#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../draw/server.h"

struct media_base;
struct snd_obj;
struct snd_stream;
struct strmd_snd;

struct snd_list {
	struct snd_obj **items;
	int32_t count;
};

struct snd_stream_ops {
	struct server_ops base;
	void (*Pause)(struct snd_stream *self);
	void (*Resume)(struct snd_stream *self);
	int32_t (*SetLevel)(struct snd_stream *self, int32_t level);
};

struct snd_stream {
	struct server base;
	int32_t unused_E;
	int32_t level;
	struct snd_list *streams;
	bool enabled;
};

#define SND_STREAM_OPS(p) ((const struct snd_stream_ops *) ((const struct object *) (p))->ops)

extern const struct snd_stream_ops SND_STREAM_OPS_TABLE;

void SndStreamInit(struct snd_stream *self, struct server *source);
void snd_stream_destroy(struct object *self);

struct media_base *snd_stream_acquire_object(struct server *self, int32_t key, bool flag);
struct media_base *snd_stream_create_object(struct server *self, int32_t key, bool flag);
void snd_stream_pause(struct snd_stream *self);
void snd_stream_resume(struct snd_stream *self);
int32_t snd_stream_set_level(struct snd_stream *self, int32_t level);

void SndStreamMarkChain(struct snd_stream *self);
void SndStreamStopAll(struct snd_stream *self);
void SndStreamStartStream(struct snd_stream *self, int32_t key_hi, int32_t key_lo, bool flag);

extern struct strmd_snd *g_sound_server;
