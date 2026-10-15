#pragma once

#include <stdint.h>

#include "../base/object.h"

struct font_ops {
	struct object_ops base;
};

struct font {
	struct object base;
	int32_t font_id;
	void *hfont;
	int32_t style;
	int32_t point_size;
};

#define FONT_OPS(p) ((const struct font_ops *) ((const struct object *) (p))->ops)

extern const struct font_ops FONT_OPS_TABLE;

void FontInit(struct font *self, int32_t font_id, void *hfont, int32_t point_size, int32_t style);
struct font *FontCreate(int32_t font_id, void *hfont, int32_t point_size, int32_t style);
void FontDestroy(struct font **ctx_mod);
void font_destroy(struct object *self);
void *FontCreateHandle(const char *face_name, int32_t point_size, int32_t style);
