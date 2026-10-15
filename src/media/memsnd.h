#pragma once

#include <stdint.h>

#include "sndbase.h"

struct mem_snd;
struct server;

struct mem_snd_ops {
	struct snd_base_ops base;
};

struct mem_snd {
	struct snd_base base;
	uint8_t *mem;
	int32_t mem_size;
	void *data;
};

#define MEM_SND_OPS(p) ((const struct mem_snd_ops *) ((const struct object *) (p))->ops)

extern const struct mem_snd_ops MEM_SND_OPS_TABLE;

void MemSndInit(struct mem_snd *self, struct server *source, uint8_t *mem, int32_t mem_size);
struct mem_snd *MemSndCreate(struct server *source, uint8_t *mem, int32_t mem_size);
void mem_snd_destroy(struct object *self);

int32_t mem_snd_start_playback(struct snd_obj *self);
