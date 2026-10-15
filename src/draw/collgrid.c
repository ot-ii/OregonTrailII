#include "collgrid.h"

#include <string.h>

#include "../base/memory.h"

const struct coll_grid_ops COLL_GRID_OPS_TABLE = {
	{
		{coll_grid_destroy},
		coll_grid_reset,
		coll_grid_count,
		coll_grid_get_rect_at,
		coll_grid_add_rect,
		coll_grid_intersects_rect,
	},
};

void CollGridInit(struct coll_grid *self, int32_t cols, int32_t rows)
{
	ShapeBaseInit(&self->base);
	self->base.base.ops = &COLL_GRID_OPS_TABLE.base.base;

	self->cols = cols;
	self->rows = rows;
	self->grid_size = self->cols * self->rows;
	self->grid = MemAlloc((size_t) (self->grid_size > 0 ? self->grid_size : 0), sizeof(int32_t));
	SHAPE_BASE_OPS(self)->Reset(&self->base);
	self->rect.left = 0;
	self->rect.top = self->rect.left;
	self->rect.bottom = 16 * self->rows;
	self->rect.right = 16 * self->cols;
}

struct coll_grid *CollGridCreate(int32_t cols, int32_t rows)
{
	struct coll_grid *self = MemAlloc(1, sizeof(struct coll_grid));
	CollGridInit(self, cols, rows);

	return self;
}

void CollGridDestroy(struct coll_grid **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void coll_grid_destroy(struct object *obj)
{
	struct coll_grid *self = (struct coll_grid *) obj;

	MemFree(self->grid);
	self->grid = NULL;

	obj->ops = &SHAPE_BASE_OPS_TABLE.base;
	shape_base_destroy(obj);
}

void coll_grid_reset(struct shape_base *base)
{
	struct coll_grid *self = (struct coll_grid *) base;

	int32_t *cell = self->grid;
	int32_t cell_count = self->cols * self->rows;

	while (cell_count-- > 0) {
		*cell = 0;
		++cell;
	}
}

static struct rect cell_rect_of(struct coll_grid *self, struct rect *rect)
{
	struct rect cell_rect = *rect;

	RectIntersect(&cell_rect, &self->rect, &cell_rect);
	cell_rect.top /= 16;
	cell_rect.left /= 16;

	if (cell_rect.bottom & 0xF)
		cell_rect.bottom += 16;

	cell_rect.bottom /= 16;

	if (cell_rect.right & 0xF)
		cell_rect.right += 16;

	cell_rect.right /= 16;

	return cell_rect;
}

void coll_grid_add_rect(struct shape_base *base, struct rect *rect)
{
	struct coll_grid *self = (struct coll_grid *) base;

	struct rect cell_rect = cell_rect_of(self, rect);
	int32_t *row_start = self->grid + (self->cols * cell_rect.top + cell_rect.left);

	for (int32_t row = cell_rect.top; cell_rect.bottom > row; ++row) {
		int32_t *cell = row_start;

		for (int32_t col = cell_rect.left; cell_rect.right > col; ++col) {
			*cell = 1;
			++cell;
		}

		row_start += self->cols;
	}
}

bool coll_grid_intersects_rect(struct shape_base *base, struct rect *rect)
{
	struct coll_grid *self = (struct coll_grid *) base;

	bool hit = false;
	struct rect cell_rect = cell_rect_of(self, rect);
	int32_t *row_start = self->grid + (self->cols * cell_rect.top + cell_rect.left);

	for (int32_t row = cell_rect.top; cell_rect.bottom > row; ++row) {
		int32_t *cell = row_start;

		for (int32_t col = cell_rect.left; cell_rect.right > col; ++col) {
			if (*cell++) {
				hit = true;
				goto done;
			}
		}

		row_start += self->cols;
	}

done:
	return hit;
}

void CollGridBlitCells(struct coll_grid *self, uint8_t *src, int32_t src_row_bytes, uint8_t *dst,
	int32_t dst_row_bytes, int32_t dst_x, int32_t dst_y)
{
	int32_t *grid_row = self->grid;
	dst_row_bytes = dst_row_bytes & 0x7FFF;
	dst = dst + dst_x + dst_row_bytes * dst_y;
	intptr_t src_step = 16 * (intptr_t) src_row_bytes;
	intptr_t dst_step = 16 * (intptr_t) dst_row_bytes;

	for (int32_t row = 0; self->rows > row; ++row) {
		int32_t *cell = grid_row;
		uint8_t *dst_cell = dst;
		uint8_t *src_cell = src;

		for (int32_t col = 0; self->cols > col; ++col) {
			if (*cell++) {
				uint8_t *dst_line = dst_cell;
				uint8_t *src_line = src_cell;

				for (int32_t line_count = 16; line_count; --line_count) {
					memcpy(dst_line, src_line, 16);
					dst_line += dst_row_bytes;
					src_line += src_row_bytes;
				}
			}

			dst_cell += 16;
			src_cell += 16;
		}

		grid_row += self->cols;
		dst += dst_step;
		src += src_step;
	}
}

int32_t coll_grid_count(struct shape_base *self)
{
	return 1;
}

void coll_grid_get_rect_at(struct shape_base *base, struct rect *out, int32_t index)
{
	struct coll_grid *self = (struct coll_grid *) base;

	*out = self->rect;
}
