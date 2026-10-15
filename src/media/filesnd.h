#pragma once

#include <stdint.h>

#include "sndbase.h"

struct file_snd;
struct server;

struct file_snd_ops {
	struct snd_base_ops base;
};

struct file_snd {
	struct snd_base base;
	int32_t res_id;
};

#define FILE_SND_OPS(p) ((const struct file_snd_ops *) ((const struct object *) (p))->ops)

extern const struct file_snd_ops FILE_SND_OPS_TABLE;

void FileSndInit(struct file_snd *self, struct server *source, int32_t res_id);
struct file_snd *FileSndCreate(struct server *source, int32_t res_id);
void file_snd_destroy(struct object *self);

int32_t file_snd_start_playback(struct snd_obj *self);
