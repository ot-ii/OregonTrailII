#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "objbase.h"

struct canvas;
struct drw_egad;
struct screen_dc;

struct drw_egad_ops {
	struct obj_base_ops base;
};

struct drw_egad {
	struct obj_base base;
	struct canvas *canvas;
};

#define DRW_EGAD_OPS(p) ((const struct drw_egad_ops *) ((const struct object *) (p))->ops)

extern const struct drw_egad_ops DRW_EGAD_OPS_TABLE;

void DrwEgadInit(struct drw_egad *self, const struct rect *src);
struct drw_egad *DrwEgadCreate(const struct rect *src);
void DrwEgadDestroy(struct drw_egad **ctx_mod);
void drw_egad_destroy(struct object *self);

int32_t drw_egad_draw_to(struct obj_base *self, struct screen_dc *dc, struct point *pos, int32_t flag);
bool drw_egad_free(struct media_base *self);
bool drw_egad_release_lock(struct media_base *self);
bool drw_egad_is_unlocked(struct media_base *self);
int32_t drw_egad_get_size(struct obj_base *self, struct point *out);
struct canvas *drw_egad_get_rect(struct obj_base *self, struct rect *out);
