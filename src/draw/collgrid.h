#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "shapebase.h"

struct coll_grid_ops {
	struct shape_base_ops base;
};

struct coll_grid {
	struct shape_base base;
	int32_t cols;
	int32_t rows;
	int32_t *grid;
	int32_t grid_size;
	struct rect rect;
};

#define COLL_GRID_OPS(p) ((const struct coll_grid_ops *) ((const struct object *) (p))->ops)

extern const struct coll_grid_ops COLL_GRID_OPS_TABLE;

void CollGridInit(struct coll_grid *self, int32_t cols, int32_t rows);
struct coll_grid *CollGridCreate(int32_t cols, int32_t rows);
void CollGridDestroy(struct coll_grid **ctx_mod);
void coll_grid_destroy(struct object *self);

void coll_grid_reset(struct shape_base *self);
int32_t coll_grid_count(struct shape_base *self);
void coll_grid_get_rect_at(struct shape_base *self, struct rect *out, int32_t index);
void coll_grid_add_rect(struct shape_base *self, struct rect *rect);
bool coll_grid_intersects_rect(struct shape_base *self, struct rect *rect);

void CollGridBlitCells(struct coll_grid *self, uint8_t *src, int32_t src_row_bytes, uint8_t *dst,
	int32_t dst_row_bytes, int32_t dst_x, int32_t dst_y);
