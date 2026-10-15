#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sndobj.h"

struct server;
struct snd_base;

struct snd_base_ops {
	struct snd_obj_ops base;
};

struct snd_base {
	struct snd_obj base;
	int32_t handle;
	bool looping;
};

#define SND_BASE_OPS(p) ((const struct snd_base_ops *) ((const struct object *) (p))->ops)

extern const struct snd_base_ops SND_BASE_OPS_TABLE;

void SndBaseInit(struct snd_base *self, struct server *source);
void snd_base_destroy(struct object *self);

int32_t snd_base_pause(struct snd_obj *self);
int32_t snd_base_resume(struct snd_obj *self);
int32_t snd_base_start_playback(struct snd_obj *self);
int32_t snd_base_start(struct snd_obj *self);
int32_t snd_base_resume_playback(struct snd_obj *self);
int32_t snd_base_stop(struct snd_obj *self);
int32_t snd_base_play_sync(struct snd_obj *self);
int32_t snd_base_get_status(struct snd_obj *self);
int32_t snd_base_get_level(struct snd_obj *self);
int32_t snd_base_set_level(struct snd_obj *self, int32_t volume);
