#pragma once

#include <stdint.h>

#include "mixstrm.h"

struct xfade_strm;

struct xfade_strm_ops {
	struct mix_strm_ops base;
};

struct xfade_strm {
	struct mix_strm base;
	int32_t active_channel;
};

#define XFADE_STRM_OPS(p) ((const struct xfade_strm_ops *) ((const struct object *) (p))->ops)

extern const struct xfade_strm_ops XFADE_STRM_OPS_TABLE;

void XfadeStrmInit(struct xfade_strm *self, struct snd_strm *source);
struct xfade_strm *XfadeStrmCreate(struct snd_strm *source);
void xfade_strm_destroy(struct object *self);

int32_t xfade_strm_set_selection(struct snd_strm *self, int32_t start, int32_t loop_start, int32_t end, int32_t ramp);
