#pragma once

#include <stdint.h>

#include "sndbase.h"

struct res_snd;
struct server;

struct res_snd_ops {
	struct snd_base_ops base;
};

struct res_snd {
	struct snd_base base;
	void *res_handle;
};

#define RES_SND_OPS(p) ((const struct res_snd_ops *) ((const struct object *) (p))->ops)

extern const struct res_snd_ops RES_SND_OPS_TABLE;

void ResSndInit(struct res_snd *self, struct server *source, int32_t res_id);
struct res_snd *ResSndCreate(struct server *source, int32_t res_id);
void res_snd_destroy(struct object *self);

int32_t res_snd_stop(struct snd_obj *self);
int32_t res_snd_start_playback(struct snd_obj *self);
