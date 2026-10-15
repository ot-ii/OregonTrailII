#pragma once

#include <stdint.h>

#include "../base/object.h"

struct assets;

struct font_rsrc_ops {
	struct object_ops base;
};

struct font_rsrc {
	struct object base;
	uint8_t *mem;
	int32_t mem_size;
};

#define FONT_RSRC_OPS(p) ((const struct font_rsrc_ops *) ((const struct object *) (p))->ops)

extern const struct font_rsrc_ops FONT_RSRC_OPS_TABLE;

void FontRsrcInit(struct font_rsrc *self, struct assets *lib, int32_t id);
struct font_rsrc *FontRsrcCreate(struct assets *lib, int32_t id);
void FontRsrcDestroy(struct font_rsrc **ctx_mod);
void font_rsrc_destroy(struct object *self);
