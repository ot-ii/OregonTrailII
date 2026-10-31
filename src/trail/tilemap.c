#include "tilemap.h"

#include <stddef.h>
#include <string.h>

#include "../base/byteorder.h"
#include "../draw/canvas.h"
#include "../draw/collgrid.h"
#include "../base/list.h"
#include "../base/memory.h"
#include "../draw/rectrgnlist.h"
#include "../draw/sprobj.h"

const struct tile_map_ops TILE_MAP_OPS_TABLE = {
	{
		{tile_map_destroy},
		tile_map_render_pipeline,
		view_surface_set_background_source,
		view_surface_set_cover_mask,
		view_surface_head,
		view_surface_find_by_id,
		view_surface_hit_test,
		view_surface_add_child,
		view_surface_remove_child,
		view_surface_mark_all_dirty,
		view_surface_remove_all_children,
		view_surface_purge_dead,
		view_surface_draw_region,
		view_surface_mark_hit,
		view_surface_flush_to_window,
		view_surface_redraw_dirty,
		tile_map_blit,
		view_surface_for_each_child,
	},
	tile_map_scroll_redraw,
	tile_map_dispatch_to_child_views,
	tile_map_scroll_to,
	tile_map_blit_or_invalidate,
};

static struct shape_base *rgn(struct rect_rgn_list *list)
{
	return &list->base;
}

static int32_t map_width(const struct tile_map *self)
{
	return (int32_t) ReadLeU16(self->map_data);
}

static int32_t map_height(const struct tile_map *self)
{
	return ReadLe16(self->map_data + 2);
}

static const uint8_t *map_cells(const struct tile_map *self)
{
	return self->map_data + 4;
}

static const uint8_t *tile_source(const struct tile_map *self, uint32_t *tile_id)
{
	if (self->first_shore_tile > (int32_t) *tile_id && *tile_id != RAFT_LAND)
		*tile_id = 0;

	return self->tile_sheet + ((size_t) *tile_id << 8);
}

static void copy_tile(uint8_t *dst, const uint8_t *src, int32_t stride)
{
	for (int32_t rows = 16; rows; rows--) {
		memcpy(dst, src, 16);
		src += 16;
		dst += stride;
	}
}

void TileMapInit(struct tile_map *self, struct rect *bounds, struct palette *palette, const struct rect *src_rect,
	const uint8_t *map_data, const uint8_t *tile_sheet, int32_t first_shore_tile)
{
	ViewSurfaceInit(&self->base, bounds, palette, false, false);
	self->base.base.ops = &TILE_MAP_OPS_TABLE.base.base;

	self->scroll_dirty = true;
	self->map_rect = *src_rect;
	self->map_origin = RectTopLeft(&self->map_rect);
	self->map_rect.right = self->map_rect.right - self->map_rect.left;
	self->map_rect.bottom = self->map_rect.bottom - self->map_rect.top;
	self->map_rect.top = 0;
	self->map_rect.left = 0;
	self->map_data = map_data;
	self->tile_sheet = tile_sheet;
	self->first_shore_tile = first_shore_tile;
	self->last_scroll.h = -999;
	self->last_scroll.v = -999;

	self->cw4 = (self->map_rect.right - self->map_rect.left + 3) / 4;
	self->ch4 = (self->map_rect.bottom - self->map_rect.top + 3) / 4;
	self->cells4 = MemAlloc((size_t) self->cw4 * (size_t) self->ch4, sizeof(int32_t));

	int32_t cnt = (self->map_rect.right - self->map_rect.left) * (self->map_rect.bottom - self->map_rect.top);
	self->tiles = MemAlloc((size_t) cnt, sizeof(int32_t));

	for (int32_t i = 0; cnt > i; ++i)
		self->tiles[i] = -1;

	self->grid = CollGridCreate((self->base.rect_a.right - self->base.rect_a.left) / 16,
		(self->base.rect_a.bottom - self->base.rect_a.top) / 16);
}

struct tile_map *TileMapCreate(struct rect *bounds, struct palette *palette, const struct rect *src_rect,
	const uint8_t *map_data, const uint8_t *tile_sheet, int32_t first_shore_tile)
{
	struct tile_map *self = MemAlloc(1, sizeof(struct tile_map));
	TileMapInit(self, bounds, palette, src_rect, map_data, tile_sheet, first_shore_tile);

	return self;
}

void TileMapDestroy(struct tile_map **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void tile_map_destroy(struct object *obj)
{
	struct tile_map *self = (struct tile_map *) obj;

	MemFree(self->cells4);
	self->cells4 = NULL;
	MemFree(self->tiles);
	self->tiles = NULL;
	CollGridDestroy(&self->grid);

	obj->ops = &VIEW_SURFACE_OPS_TABLE.base;
	view_surface_destroy(obj);
}

void tile_map_blit(struct view_surface *vs, struct rect *rect)
{
	struct tile_map *self = (struct tile_map *) vs;

	int32_t width = map_width(self);
	struct canvas_bits *bits = &vs->main_canvas->bits;

	struct rect tile_rect;
	RectIntersect(rect, &vs->rect_a, &tile_rect);
	tile_rect.top /= 16;
	tile_rect.left /= 16;

	if (tile_rect.bottom & 0xF)
		tile_rect.bottom += 16;

	tile_rect.bottom /= 16;

	if (tile_rect.right & 0xF)
		tile_rect.right += 16;

	tile_rect.right /= 16;

	struct rect view_tiles = tile_rect;
	int32_t scroll_col = self->scroll_pos.h / 16;
	int32_t scroll_row = self->scroll_pos.v / 16;
	view_tiles.left = scroll_col + view_tiles.left;
	view_tiles.right = scroll_col + view_tiles.right;
	view_tiles.top = scroll_row + view_tiles.top;
	view_tiles.bottom = scroll_row + view_tiles.bottom;

	CanvasLock(vs->main_canvas);

	intptr_t stride = bits->stride & 0x7FFF;

	const uint8_t *row_tiles = map_cells(self) + 2 * ((intptr_t) view_tiles.top * width + view_tiles.left);
	uint8_t *row_pixels = bits->bits + self->map_origin.v * stride * 16 + self->map_origin.h * 16;
	row_pixels += 16 * tile_rect.top * stride + 16 * tile_rect.left;

	for (int32_t tile_row = view_tiles.top; view_tiles.bottom > tile_row; ++tile_row) {
		const uint8_t *tile_cursor = row_tiles;
		uint8_t *dst = row_pixels;

		for (int32_t tile_col = view_tiles.left; view_tiles.right > tile_col; ++tile_col) {
			uint32_t tile_id = ReadLeU16(tile_cursor);
			tile_cursor += 2;
			copy_tile(dst, tile_source(self, &tile_id), (int32_t) stride);
			dst += 16;
		}

		row_tiles += 2 * (intptr_t) width;
		row_pixels += 16 * stride;
	}

	CanvasUnlock(vs->main_canvas);
}

void tile_map_scroll_redraw(struct tile_map *self, struct rect_rgn_list *region, bool use_dirty_buffer)
{
	struct view_surface *vs = &self->base;
	int32_t width = map_width(self);
	struct canvas_bits *bits = &vs->main_canvas->bits;
	int32_t *dirty_flags;

	if (PointEqual(self->scroll_pos, self->last_scroll))
		return;

	self->last_scroll = self->scroll_pos;

	if (!use_dirty_buffer) {
		for (int32_t row = 0; self->ch4 > row; ++row) {
			for (int32_t col = 0; self->cw4 > col; ++col)
				self->cells4[row * self->cw4 + col] = 0;
		}

		dirty_flags = NULL;

	} else {
		dirty_flags = ((struct coll_grid *) (void *) region)->grid;
	}

	struct rect view_tiles = self->map_rect;
	int32_t scroll_col = self->scroll_pos.h / 16;
	int32_t scroll_row = self->scroll_pos.v / 16;
	view_tiles.left = view_tiles.left + scroll_col;
	view_tiles.right = view_tiles.right + scroll_col;
	view_tiles.top = view_tiles.top + scroll_row;
	view_tiles.bottom = view_tiles.bottom + scroll_row;

	CanvasLock(vs->main_canvas);

	intptr_t stride = bits->stride & 0x7FFF;
	intptr_t row_step = stride << 4;
	int32_t *cached_tile = self->tiles;
	const uint8_t *row_tiles = map_cells(self) + 2 * ((intptr_t) view_tiles.top * width + view_tiles.left);
	uint8_t *row_pixels = bits->bits + self->map_origin.v * stride * 16 + self->map_origin.h * 16;

	for (int32_t tile_row = view_tiles.top; view_tiles.bottom > tile_row; ++tile_row) {
		const uint8_t *tile_cursor = row_tiles;
		uint8_t *dst = row_pixels;

		for (int32_t tile_col = view_tiles.left; view_tiles.right > tile_col; ++tile_col) {
			uint32_t tile_id = ReadLeU16(tile_cursor);
			const uint8_t *src = tile_source(self, &tile_id);

			if (*cached_tile != (int32_t) tile_id) {
				if (!use_dirty_buffer) {
					int32_t cell_col = (tile_col - view_tiles.left) / 4;
					int32_t cell_row = (tile_row - view_tiles.top) / 4;
					++self->cells4[self->cw4 * cell_row + cell_col];
				}

				copy_tile(dst, src, (int32_t) stride);
				*cached_tile = (int32_t) tile_id;

				if (dirty_flags)
					*dirty_flags = 1;
			}

			++cached_tile;

			if (dirty_flags)
				++dirty_flags;

			tile_cursor += 2;
			dst += 16;
		}

		row_tiles += 2 * (intptr_t) width;
		row_pixels += row_step;
	}

	if (!use_dirty_buffer) {
		for (int32_t row = 0; self->ch4 > row; ++row) {
			for (int32_t col = 0; self->cw4 > col; ++col) {
				if (self->cells4[row * self->cw4 + col]) {
					struct rect dirty_rect = RectMake(0, 0, 0x40, 0x40);
					dirty_rect.left = (col << 6) + dirty_rect.left;
					dirty_rect.right = (col << 6) + dirty_rect.right;
					dirty_rect.top = (row << 6) + dirty_rect.top;
					dirty_rect.bottom = (row << 6) + dirty_rect.bottom;
					dirty_rect.top -= 2;
					dirty_rect.bottom += 2;
					dirty_rect.left = dirty_rect.left - 1;
					dirty_rect.right = dirty_rect.right + 1;
					SHAPE_BASE_OPS(region)->AddRect(rgn(region), &dirty_rect);
				}
			}
		}
	}

	CanvasUnlock(vs->main_canvas);
}

void tile_map_blit_or_invalidate(struct tile_map *self, struct win_dc *windc, struct rect_rgn_list *rgn_list,
	bool invalidate_only)
{
	SHAPE_BASE_OPS(rgn_list)->Count(rgn(rgn_list));

	if (invalidate_only) {
		CanvasUnlock(self->base.main_canvas);

	} else {
		view_surface_flush_to_window(&self->base, windc, rgn_list);
	}
}

void tile_map_render_pipeline(struct view_surface *vs, struct win_dc *window, bool flush)
{
	struct tile_map *self = (struct tile_map *) vs;
	const struct view_surface_ops *ops = VIEW_SURFACE_OPS(vs);
	struct rect_rgn_list *clip = vs->clip_list;

	self->use_dirty_buffer = false;
	ops->ForEachChild(vs);
	SHAPE_BASE_OPS(clip)->Reset(rgn(clip));

	if (self->scroll_dirty)
		TILE_MAP_OPS(self)->ScrollRedraw(self, clip, self->use_dirty_buffer);

	self->scroll_dirty = false;
	ops->DrawRegion(vs, clip);
	ops->MarkHit(vs, clip);
	ops->RedrawDirty(vs, clip);

	if (!self->use_dirty_buffer) {
		SHAPE_BASE_OPS(clip)->Reset(rgn(clip));
		SHAPE_BASE_OPS(clip)->AddRect(rgn(clip), &vs->rect_a);
	}

	if (window && flush)
		TILE_MAP_OPS(self)->BlitOrInvalidate(self, window, clip, self->use_dirty_buffer);
}

void tile_map_dispatch_to_child_views(struct tile_map *self, int32_t dh, int32_t dv)
{
	struct spr_obj *child = SprObjFromNode(self->base.children->head);

	while (child) {
		struct rect tmp;
		SPR_OBJ_OPS(child)->MoveBy(child, dh, dv);

		if (!RectIntersect(&self->base.rect_a, &child->region1.rect, &tmp)) {
			SPR_OBJ_OPS(child)->Hide(child);

		} else {
			SPR_OBJ_OPS(child)->Show(child);
		}

		child = SprObjFromNode(child->node.next);
	}
}

void tile_map_scroll_to(struct tile_map *self, int32_t h, int32_t v)
{
	int32_t last_h = 16 * (map_width(self) - self->map_rect.right);
	int32_t last_v = 16 * (map_height(self) - self->map_rect.bottom);

	if (v > last_v)
		v = last_v;

	if (h > last_h)
		h = last_h;

	if (h < 0)
		h = 0;

	if (v < 16)
		v = 16;

	struct point pt;
	pt.h = h - self->scroll_pos.h;
	pt.v = v - self->scroll_pos.v;

	if (pt.h != 0 || pt.v != 0) {
		self->scroll_pos.v = v;
		self->scroll_pos.h = h;
		self->scroll_dirty = true;
		TILE_MAP_OPS(self)->DispatchToChildViews(self, -pt.h, -pt.v);
	}
}
