#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "mediabase.h"

struct canvas;
struct obj_base;
struct screen_dc;
struct server;

struct obj_base_ops {
	struct media_base_ops base;
	int32_t (*DrawTo)(struct obj_base *self, struct screen_dc *dc, struct point *pos, int32_t flag);
	int32_t (*GetSize)(struct obj_base *self, struct point *out);
	int32_t (*Slot9)(struct obj_base *self);
	int32_t (*Slot10)(struct obj_base *self, int32_t arg);
	struct canvas *(*GetRect)(struct obj_base *self, struct rect *out);
};

struct obj_base {
	struct media_base base;
	int32_t reserved_10;
};

#define OBJ_BASE_OPS(p) ((const struct obj_base_ops *) ((const struct object *) (p))->ops)

extern const struct obj_base_ops OBJ_BASE_OPS_TABLE;

void ObjBaseInit(struct obj_base *self, struct server *server);
void obj_base_destroy(struct object *self);

int32_t obj_base_get_size(struct obj_base *self, struct point *out);
int32_t obj_base_draw_to(struct obj_base *self, struct screen_dc *dc, struct point *pos, int32_t flag);
int32_t obj_base_slot9(struct obj_base *self);
int32_t obj_base_slot10(struct obj_base *self, int32_t arg);
struct canvas *obj_base_get_rect(struct obj_base *self, struct rect *out);
