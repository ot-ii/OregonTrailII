#include "canvas.h"

#include <stddef.h>
#include <string.h>

#include "../data/decode.h"
#include "../data/image.h"
#include "../base/memory.h"
#include "palette.h"
#include "../app/replay.h"
#include "screendc.h"

const struct canvas_ops CANVAS_OPS_TABLE = {
	{
		{canvas_destroy},
		canvas_draw_brush20,
		shape_draw_brush12,
		shape_draw_outline,
		shape_draw_invert,
		canvas_draw,
		shape_get_rect_by_val,
		shape_set_rect,
		shape_make_region,
	},
	canvas_stretch_blit,
	canvas_alpha_blend,
	canvas_draw_to_device,
	canvas_draw_at,
	canvas_blit_masked,
	canvas_blit_image,
	canvas_setup_buffer,
};

static struct canvas *g_memblk_lru_head;
static struct canvas *g_memblk_lru_tail;
static int32_t g_mem_limit = 1 * 1024 * 1024;

void CanvasDibLocked(struct canvas *self)
{
	++self->dib_lock_count;
}

void CanvasDibUnlocked(struct canvas *self)
{
	if (self->dib_lock_count == 0)
		return;

	--self->dib_lock_count;
}

void CanvasLock(struct canvas *self)
{
	if (g_use_dib_sections && self->backing_dc)
		self->bits.bits = self->backing_dc->bits;

	if (self->dib) {
		CanvasDibLocked(self);
		self->bits.bits = self->dib_size ? self->dib : NULL;
	}
}

void CanvasUnlock(struct canvas *self)
{
	if (self->backing_dc) {
		self->bits.bits = NULL;

	} else if (self->dib) {
		CanvasDibUnlocked(self);
		self->bits.bits = NULL;
	}
}

static void dib_unlink_lru(struct canvas *self)
{
	if (!self->dib_in_lru)
		return;

	if (self->dib_lru_next) {
		self->dib_lru_next->dib_lru_prev = self->dib_lru_prev;

	} else {
		g_memblk_lru_tail = self->dib_lru_prev;
	}

	if (self->dib_lru_prev) {
		self->dib_lru_prev->dib_lru_next = self->dib_lru_next;

	} else {
		g_memblk_lru_head = self->dib_lru_next;
	}

	self->dib_lru_next = NULL;
	self->dib_lru_prev = NULL;
	self->dib_in_lru = false;
}

static void dib_free(struct canvas *self)
{
	self->dib_size = 0;
	dib_unlink_lru(self);
}

static int32_t memblk_purge_one(void)
{
	int32_t freed = 0;

	while (freed == 0) {
		struct canvas *block;

		for (block = g_memblk_lru_head; block != NULL; block = block->dib_lru_next) {
			if (block->dib_lock_count != 0)
				continue;

			break;
		}

		if (block == NULL)
			break;

		freed = block->dib_size;
		dib_free(block);
	}

	return freed;
}

static void dib_make_purgeable(struct canvas *self)
{
	if (self->dib_in_lru)
		return;

	self->dib_lru_prev = g_memblk_lru_tail;
	self->dib_lru_next = NULL;
	self->dib_in_lru = true;

	if (g_memblk_lru_tail != NULL) {
		g_memblk_lru_tail->dib_lru_next = self;

	} else {
		g_memblk_lru_head = self;
	}

	g_memblk_lru_tail = self;
	int32_t total = 0;

	for (struct canvas *block = g_memblk_lru_head; block != NULL; block = block->dib_lru_next) {
		if (block->dib_lock_count == 0) {
			if (block->dib_size != 0)
				total += block->dib_size;
		}
	}

	while (g_mem_limit < total) {
		int32_t purged = memblk_purge_one();

		if (purged == 0)
			break;

		total -= purged;
	}
}

void CanvasSetPurgeable(struct canvas *self, bool flag)
{
	if (!flag && self->dib_purgeable) {
		dib_unlink_lru(self);

	} else if (flag && !self->dib_purgeable) {
		dib_make_purgeable(self);
	}

	self->dib_purgeable = flag;
}

bool CanvasDibInvalid(struct canvas *self)
{
	return self->dib_size == 0;
}

static void blit_copy_rect(struct canvas_bits *dst, struct canvas_bits *src, struct rect *src_rect,
	struct rect *dst_rect, int32_t mode, struct rect *clip)
{
	(void) mode;

	int32_t from_skip = dst->stride;
	int32_t to_skip = src->stride;
	from_skip &= 0x7FFF;
	to_skip &= 0x7FFF;

	struct rect clipped;
	clipped.left = clip->left <= dst_rect->left ? dst_rect->left : clip->left;
	clipped.right = clip->right >= dst_rect->right ? dst_rect->right : clip->right;
	clipped.top = clip->top <= dst_rect->top ? dst_rect->top : clip->top;
	clipped.bottom = clip->bottom >= dst_rect->bottom ? dst_rect->bottom : clip->bottom;

	clipped.left = src->rect.left <= clipped.left ? clipped.left : src->rect.left;
	clipped.right = src->rect.right >= clipped.right ? clipped.right : src->rect.right;
	clipped.top = src->rect.top <= clipped.top ? clipped.top : src->rect.top;
	clipped.bottom = src->rect.bottom >= clipped.bottom ? clipped.bottom : src->rect.bottom;

	int32_t width = clipped.right - clipped.left;
	int32_t dword_count = width / 4;
	int32_t byte_count = width % 4;
	int32_t rows = clipped.bottom - clipped.top;

	if (width < 0 || rows < 0)
		return;

	uint8_t *from_pos = (uint8_t *) dst->bits + (src_rect->left - dst->rect.left) +
		(intptr_t) from_skip * (src_rect->top - dst->rect.top);
	uint8_t *to_pos = (uint8_t *) src->bits + (clipped.left - src->rect.left) +
		(intptr_t) to_skip * (clipped.top - src->rect.top);
	from_skip -= width;
	to_skip -= width;

	for (int32_t row = 0; row < rows; row++) {
		for (int32_t i = 0; i < dword_count; i++) {
			uint8_t quad[4];
			memcpy(quad, from_pos, 4);
			memcpy(to_pos, quad, 4);
			from_pos += 4;
			to_pos += 4;
		}

		for (int32_t i = 0; i < byte_count; i++)
			*to_pos++ = *from_pos++;

		from_pos += from_skip;
		to_pos += to_skip;
	}
}

static void blit_transparent(uint8_t *dst, uint8_t *src, int32_t dst_stride, int32_t src_stride, int32_t width,
	int32_t rows)
{
	if (!width)
		return;

	uint8_t *from = dst;
	uint8_t *to = src;
	dst_stride -= width;
	src_stride -= width;

	for (int32_t row = 0; row < rows; row++) {
		for (int32_t col = 0; col < width; col++) {
			uint8_t px = *from++;

			if (px)
				*to = px;

			++to;
		}

		from += dst_stride;
		to += src_stride;
	}
}

static void blit_masked(uint8_t *dst, uint8_t *src, uint8_t *mask, int32_t dst_stride, int32_t src_stride,
	int32_t width, int32_t rows, int32_t threshold)
{
	if (!width)
		return;

	uint8_t *from = dst;
	uint8_t *to = src;
	uint8_t *mask_run = mask;
	dst_stride -= width;
	src_stride -= width;

	for (int32_t row = 0; row < rows; row++) {
		for (int32_t col = 0; col < width; col++) {
			uint8_t *mask_px = mask_run++;

			if (*mask_px < threshold) {
				if (*from > 0)
					*to = *from;
			}

			++to;
			++from;
		}

		from += dst_stride;
		to += src_stride;
		mask_run += src_stride;
	}
}

static void blit_transparent_xflip(uint8_t *dst, uint8_t *src, int32_t dst_stride, int32_t src_stride,
	int32_t width, int32_t rows)
{
	if (!width)
		return;

	uint8_t *from = dst;
	uint8_t *to = src;
	dst_stride += width;
	src_stride -= width;

	for (int32_t row = 0; row < rows; row++) {
		for (int32_t col = 0; col < width; col++) {
			uint8_t px = *from--;

			if (px)
				*to = px;

			++to;
		}

		from += dst_stride;
		to += src_stride;
	}
}

static void blit_masked_flipped(uint8_t *dst, uint8_t *src, uint8_t *mask, int32_t dst_stride,
	int32_t src_stride, int32_t width, int32_t rows, int32_t threshold)
{
	if (!width)
		return;

	uint8_t *from = dst;
	uint8_t *to = src;
	uint8_t *mask_run = mask;
	dst_stride += width;
	src_stride -= width;

	for (int32_t row = 0; row < rows; row++) {
		for (int32_t col = 0; col < width; col++) {
			uint8_t *mask_px = mask_run++;

			if (*mask_px < threshold) {
				if (*from > 0)
					*to = *from;
			}

			++to;
			--from;
		}

		from += dst_stride;
		to += src_stride;
		mask_run += src_stride;
	}
}

void CanvasInit(struct canvas *self)
{
	ShapeInit(&self->base, 0, 255, 255, 1, 0);
	self->base.base.ops = &CANVAS_OPS_TABLE.base.base;
	self->bounds = RectMake(0, 0, 0, 0);
	self->bits.valid = 0;
	self->dib = NULL;
	self->dib_size = 0;
	self->dib_lock_count = 0;
	self->dib_purgeable = false;
	self->dib_in_lru = false;
	self->dib_lru_prev = NULL;
	self->dib_lru_next = NULL;
	self->backing_dc = NULL;
}

void CanvasInitRect(struct canvas *self, struct rect *rc, int32_t depth, uint8_t clear_color)
{
	ShapeInit(&self->base, 0, 255, 255, 1, 0);
	self->base.base.ops = &CANVAS_OPS_TABLE.base.base;
	self->bounds = RectMake(0, 0, 0, 0);
	canvas_setup_buffer(self, rc, NULL, NULL, depth, clear_color, 0);
}

void CanvasInitAt(struct canvas *self, struct rect *rc, struct point *pt, int32_t depth, uint8_t clear_color,
	int32_t blit_mode)
{
	ShapeInit(&self->base, 0, 255, 255, 1, 0);
	self->base.base.ops = &CANVAS_OPS_TABLE.base.base;
	self->bounds = RectMake(0, 0, 0, 0);

	struct rect bounds = *rc;
	int32_t width = bounds.right - bounds.left;
	int32_t height = bounds.bottom - bounds.top;
	bounds.top = pt->v;
	bounds.left = pt->h;
	bounds.right = pt->h + width;
	bounds.bottom = pt->v + height;

	canvas_setup_buffer(self, rc, &bounds, NULL, depth, clear_color, blit_mode);
}

void CanvasInitFull(struct canvas *self, struct rect *rc, struct rect *shape_rect, struct rect *bounds_rect,
	int32_t depth, uint8_t clear_color, int32_t blit_mode, int32_t pen_color, int32_t brush_color,
	int32_t alt_brush_color, int32_t pen_width, int32_t pen_style)
{
	ShapeInit(&self->base, pen_color, brush_color, alt_brush_color, pen_width, pen_style);
	self->base.base.ops = &CANVAS_OPS_TABLE.base.base;
	self->bounds = RectMake(0, 0, 0, 0);
	canvas_setup_buffer(self, rc, shape_rect, bounds_rect, depth, clear_color, blit_mode);
}

struct canvas *CanvasCreate(void)
{
	struct canvas *ctx = MemAlloc(1, sizeof(struct canvas));
	CanvasInit(ctx);

	return ctx;
}

struct canvas *CanvasCreateRect(struct rect *rc, int32_t depth, uint8_t clear_color)
{
	struct canvas *ctx = MemAlloc(1, sizeof(struct canvas));
	CanvasInitRect(ctx, rc, depth, clear_color);

	return ctx;
}

struct canvas *CanvasCreateAt(struct rect *rc, struct point *pt, int32_t depth, uint8_t clear_color,
	int32_t blit_mode)
{
	struct canvas *ctx = MemAlloc(1, sizeof(struct canvas));
	CanvasInitAt(ctx, rc, pt, depth, clear_color, blit_mode);

	return ctx;
}

struct canvas *CanvasCreateFull(struct rect *rc, struct rect *shape_rect, struct rect *bounds_rect, int32_t depth,
	uint8_t clear_color, int32_t blit_mode, int32_t pen_color, int32_t brush_color, int32_t alt_brush_color,
	int32_t pen_width, int32_t pen_style)
{
	struct canvas *ctx = MemAlloc(1, sizeof(struct canvas));
	CanvasInitFull(ctx, rc, shape_rect, bounds_rect, depth, clear_color, blit_mode, pen_color, brush_color,
		alt_brush_color, pen_width, pen_style);

	return ctx;
}

void CanvasDestroy(struct canvas **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void canvas_destroy(struct object *self)
{
	struct canvas *c = (struct canvas *) self;

	if (c->dib) {
		dib_unlink_lru(c);
		MemFree(c->dib);
		c->dib = NULL;
		c->dib_size = 0;
	}

	self->ops = &SHAPE_OPS_TABLE.base;
	shape_destroy(self);
}

void CanvasFill(struct canvas *self, uint8_t color)
{
	if (self->dib) {
		memset(self->dib, color, (size_t) self->dib_size);

	} else {
		uint8_t *pixel_base = self->bits.bits;

		if (pixel_base) {
			int32_t count = (self->bits.rect.bottom - self->bits.rect.top) * self->bits.stride;

			while (--count >= 0)
				*pixel_base++ = color;
		}
	}
}

void CanvasFillRect(struct canvas *self, struct rect *rect, uint8_t pat)
{
	int32_t height = rect->bottom - rect->top;
	int32_t width = rect->right - rect->left;

	CanvasLock(self);

	uint8_t *dst_pos = self->bits.bits + (intptr_t) self->bits.stride * rect->top + rect->left;

	for (int32_t row = 0; height > row; ++row) {
		uint8_t *row_run = dst_pos;
		int32_t col = width;

		while (--col >= 0)
			*row_run++ = pat;

		dst_pos += self->bits.stride;
	}

	CanvasUnlock(self);
}

void canvas_draw_brush20(struct shape *self, struct screen_dc *dc)
{
	SHAPE_OPS(self)->Draw(self, dc);
	SHAPE_OPS(self)->DrawOutline(self, dc);
}

void canvas_stretch_blit(struct canvas *self, struct canvas *src_canvas, struct rect *src_rect,
	struct rect *dst_rect, int32_t mode, struct rect *clip_rect)
{
	struct canvas_bits *src_bits = &src_canvas->bits;
	struct canvas_bits *this_bits = &self->bits;
	int32_t saved_src_stride = src_bits->stride;
	int32_t saved_this_stride = this_bits->stride;
	struct rect clip;

	if (!clip_rect) {
		clip = this_bits->rect;

	} else {
		clip = *clip_rect;
	}

	CanvasLock(self);
	CanvasLock(src_canvas);

	if (src_bits->depth > 1)
		src_bits->stride |= 0x8000;

	if (this_bits->depth > 1)
		this_bits->stride |= 0x8000;

	struct rect src_arg = *src_rect;
	struct rect dst_arg = *dst_rect;
	blit_copy_rect(src_bits, this_bits, &src_arg, &dst_arg, mode, &clip);
	src_bits->stride = saved_src_stride;
	this_bits->stride = saved_this_stride;

	CanvasUnlock(src_canvas);
	CanvasUnlock(self);
}

void canvas_alpha_blend(struct canvas *self, struct canvas *src_canvas, struct canvas *alpha_canvas,
	struct point *pos, struct rect *blend_rect, int32_t unused, struct rect *clip_rect, struct palette *palette)
{
	(void) unused;

	struct canvas_bits *src_bits = &src_canvas->bits;
	struct canvas_bits *this_bits = &self->bits;
	struct canvas_bits *alpha_bits = &alpha_canvas->bits;
	int32_t src_stride = src_bits->stride;
	int32_t this_stride = this_bits->stride;
	int32_t alpha_stride = alpha_bits->stride;
	uint8_t dst_rgb[3];
	uint8_t blend[3];
	struct rect clip;

	if (!clip_rect) {
		clip = this_bits->rect;

	} else {
		clip = *clip_rect;
	}

	CanvasLock(self);
	CanvasLock(src_canvas);
	CanvasLock(alpha_canvas);

	struct rect area;
	uint8_t src_rgb[3];
	uint64_t msg[8];

	RectIntersect(blend_rect, &clip, &area);

	uint8_t *src_pos = src_bits->bits + pos->h + (intptr_t) src_stride * pos->v;
	uint8_t *dst_pos = this_bits->bits + area.left + (intptr_t) this_stride * area.top;
	uint8_t *alpha_pos = alpha_bits->bits + pos->h + (intptr_t) alpha_stride * pos->v;

	PaletteLockInvTable(palette);

	for (int32_t row = area.top; area.bottom > row; ++row) {
		uint8_t *src_run = src_pos;
		uint8_t *alpha_row = alpha_pos;
		uint8_t *dst_run = dst_pos;

		for (int32_t col = area.left; area.right > col; ++col) {
			if (!*alpha_row) {
				++dst_run;
				++src_run;

			} else if (*alpha_row == 255) {
				*dst_run = *src_run;
				++src_run;
				++dst_run;

			} else {
				PaletteIndexToRGB(palette, *dst_run, &dst_rgb[0], &dst_rgb[2], &dst_rgb[1]);
				PaletteIndexToRGB(palette, *src_run, &src_rgb[0], &src_rgb[2], &src_rgb[1]);
				blend[0] = (uint8_t) (((255 - *alpha_row) * dst_rgb[0] + src_rgb[0] * *alpha_row) / 255);
				blend[1] = (uint8_t) (((255 - *alpha_row) * dst_rgb[1] + src_rgb[1] * *alpha_row) / 255);
				blend[2] = (uint8_t) (((255 - *alpha_row) * dst_rgb[2] + src_rgb[2] * *alpha_row) / 255);
				*dst_run = PaletteRGBToIndex(palette, blend[0], blend[2], blend[1]);
				++dst_run;
				++src_run;
			}

			++alpha_row;
		}

		if (RpPeekMessage(msg, NULL, 0x3BD, 0x3BD, 1))
			RpDispatchMessage(msg);

		src_pos += src_stride;
		dst_pos += this_stride;
		alpha_pos += alpha_stride;
	}

	PaletteUnlockInvTable(palette);

	CanvasUnlock(alpha_canvas);
	CanvasUnlock(src_canvas);
	CanvasUnlock(self);
}

uint8_t CanvasGetPixel(struct canvas *self, int32_t x, int32_t y)
{
	CanvasLock(self);

	uint8_t pixel = *(self->bits.bits + ((intptr_t) self->bits.stride * y + x));

	CanvasUnlock(self);

	return pixel;
}

void canvas_setup_buffer(struct canvas *self, struct rect *rc, struct rect *shape_rect, struct rect *bounds_rect,
	int32_t depth, uint8_t clear_color, int32_t blit_mode)
{
	self->blit_mode = blit_mode;

	if (shape_rect) {
		SHAPE_OPS(self)->SetRect(&self->base, shape_rect);

	} else {
		struct rect shape = *rc;
		shape.right -= shape.left;
		shape.bottom -= shape.top;
		shape.top = 0;
		shape.left = shape.top;
		SHAPE_OPS(self)->SetRect(&self->base, &shape);
	}

	if (bounds_rect) {
		self->bounds = *bounds_rect;

	} else {
		self->bounds = *rc;
	}

	self->dib = NULL;
	self->dib_size = 0;
	self->bits.valid = 0;
	self->bits.bits = NULL;
	self->bits.depth = depth;
	self->bits.rect = *rc;

	self->bits.stride = (int32_t) (((uint32_t) ((rc->right - rc->left) * depth + 63) & ~0x3Fu) >> 3);

	if (self->bits.stride > 16382)
		return;

	self->backing_dc = NULL;

	uint32_t rows = (uint32_t) ((rc->bottom - rc->top) + 7) & ~7u;
	self->dib_size = (int32_t) (rows * (uint32_t) self->bits.stride);
	self->dib = MemAlloc((size_t) self->dib_size, 1);
	self->dib_lock_count = 0;
	self->dib_purgeable = false;
	self->dib_in_lru = false;
	self->dib_lru_prev = NULL;
	self->dib_lru_next = NULL;

	if (clear_color != 254)
		CanvasFill(self, clear_color);

	self->bits.valid = 1;
}

void canvas_draw(struct shape *self, struct screen_dc *dc)
{
	struct canvas *c = (struct canvas *) self;
	struct rect rc = SHAPE_OPS(self)->GetRectByVal(self);

	SCREEN_DC_OPS(dc)->BlitCanvas(dc, c, &c->bounds, &rc, c->blit_mode, NULL);
}

void canvas_draw_at(struct canvas *self, struct screen_dc *dc, struct point *pos, int32_t mode)
{
	struct rect rect = self->bits.rect;
	int32_t width = rect.right - rect.left;
	int32_t height = rect.bottom - rect.top;
	rect.top = pos->v;
	rect.left = pos->h;
	rect.right = pos->h + width;
	rect.bottom = pos->v + height;

	SCREEN_DC_OPS(dc)->BlitCanvas(dc, self, &self->bounds, &rect, mode, NULL);
}

void canvas_draw_to_device(struct canvas *self, struct screen_dc *dc, struct rect *src_rect, struct rect *dst_rect,
	int32_t mode, struct rect *clip_rect)
{
	SCREEN_DC_OPS(dc)->BlitCanvas(dc, self, src_rect, dst_rect, mode, clip_rect);
}

void canvas_blit_masked(struct canvas *self, struct canvas *dst_canvas, struct canvas *src_canvas,
	struct rect *dst_rect, struct point *pos, struct rect *clip_rect, int32_t threshold, bool flip_x)
{
	struct canvas_bits *dst_bits = &dst_canvas->bits;
	struct canvas_bits *this_bits = &self->bits;
	struct canvas_bits *mask_bits = &src_canvas->bits;
	struct rect blit_rect = RectMake(0, 0, 0, 0);
	struct rect dst_area = dst_bits->rect;
	struct rect src_area;
	struct rect clip;

	if (!clip_rect) {
		clip = this_bits->rect;

	} else {
		clip = *clip_rect;
	}

	CanvasLock(self);
	CanvasLock(dst_canvas);
	CanvasLock(src_canvas);

	RectIntersect(&dst_area, dst_rect, &dst_area);
	src_area = dst_area;

	RectOffset(&dst_area, pos->h - src_area.left, pos->v - src_area.top);

	struct rect this_rect = this_bits->rect;
	RectIntersect(&dst_area, &this_rect, &blit_rect);
	RectIntersect(&blit_rect, &clip, &blit_rect);

	int32_t dst_stride = dst_bits->stride & 0x7FFF;
	int32_t this_stride = this_bits->stride & 0x7FFF;
	int32_t width = blit_rect.right - blit_rect.left;
	int32_t height = blit_rect.bottom - blit_rect.top;
	int32_t dst_x;

	if (flip_x) {
		dst_x = dst_rect->right - 1 + pos->h - blit_rect.left;

	} else {
		dst_x = blit_rect.left + src_area.left - pos->h;
	}

	int32_t dst_y = src_area.top + blit_rect.top - pos->v;

	intptr_t src_off = (blit_rect.left - this_bits->rect.left) +
		(intptr_t) (blit_rect.top - this_bits->rect.top) * this_stride;
	uint8_t *src_run = this_bits->bits + src_off;
	uint8_t *mask_run = mask_bits->bits + src_off;
	uint8_t *dst_run = dst_bits->bits + (intptr_t) dst_y * dst_stride + dst_x;

	if (flip_x) {
		blit_masked_flipped(dst_run, src_run, mask_run, dst_stride, this_stride, width, height, threshold);

	} else {
		blit_masked(dst_run, src_run, mask_run, dst_stride, this_stride, width, height, threshold);
	}

	CanvasUnlock(dst_canvas);
	CanvasUnlock(src_canvas);
	CanvasUnlock(self);
}

void canvas_blit_image(struct canvas *self, struct canvas *dst_canvas, struct rect *src_rect, struct point *pos,
	struct rect *clip_rect, bool flip_x)
{
	struct canvas_bits *dst_bits = &dst_canvas->bits;
	struct canvas_bits *this_bits = &self->bits;
	struct rect blit_rect = RectMake(0, 0, 0, 0);
	struct rect shifted = RectMake(0, 0, 0, 0);
	struct rect dst_area = dst_bits->rect;
	struct rect clip;

	if (!clip_rect) {
		clip = this_bits->rect;

	} else {
		clip = *clip_rect;
	}

	CanvasLock(self);
	CanvasLock(dst_canvas);

	RectIntersect(&dst_area, src_rect, &dst_area);
	shifted = dst_area;

	RectOffset(&shifted, pos->h - dst_area.left, pos->v - dst_area.top);

	struct rect this_rect = this_bits->rect;
	RectIntersect(&shifted, &this_rect, &blit_rect);
	RectIntersect(&blit_rect, &clip, &blit_rect);

	int32_t dst_stride = dst_bits->stride & 0x7FFF;
	int32_t this_stride = this_bits->stride & 0x7FFF;
	int32_t width = blit_rect.right - blit_rect.left;
	int32_t rows = blit_rect.bottom - blit_rect.top;
	int32_t dst_x;

	if (flip_x) {
		dst_x = src_rect->right - 1 + pos->h - blit_rect.left;

	} else {
		dst_x = dst_area.left + blit_rect.left - pos->h;
	}

	int32_t dst_y = dst_area.top + blit_rect.top - pos->v;

	intptr_t src_off = (blit_rect.left - this_bits->rect.left) +
		(intptr_t) (blit_rect.top - this_bits->rect.top) * this_stride;
	uint8_t *src_run = this_bits->bits + src_off;
	uint8_t *dst_run = dst_bits->bits + (intptr_t) dst_y * dst_stride + dst_x;

	if (flip_x) {
		blit_transparent_xflip(dst_run, src_run, dst_stride, this_stride, width, rows);

	} else {
		blit_transparent(dst_run, src_run, dst_stride, this_stride, width, rows);
	}

	CanvasUnlock(dst_canvas);
	CanvasUnlock(self);
}

struct rect CanvasGetBounds(struct canvas *self)
{
	return self->bits.rect;
}

int32_t CanvasDrawFrame(struct canvas *canvas, const struct cbmp *img, int32_t frame)
{
	if (!img)
		return 1;

	if (frame < 0 || CbmpFrameCount(img) <= frame)
		return 2;

	CanvasLock(canvas);

	uint8_t *dst_bits = canvas->bits.bits;

	if (!dst_bits)
		return 0;

	CbmpDecode(img, frame, dst_bits, canvas->bits.stride, DECODE_OPAQUE, false);

	CanvasUnlock(canvas);

	return 0;
}

int32_t CanvasDrawFrameMasked(struct canvas *shape, const struct cbmp *img, int32_t frame)
{
	if (!img)
		return 1;

	if (frame < 0 || CbmpFrameCount(img) <= frame)
		return 2;

	CanvasLock(shape);

	uint8_t *dst_bits = shape->bits.bits;

	if (!dst_bits)
		return 0;

	int32_t width, height;
	CbmpFrameSize(img, frame, &width, &height);

	struct rect shape_bounds = CanvasGetBounds(shape);

	if (shape_bounds.bottom - shape_bounds.top == height && shape_bounds.right - shape_bounds.left == width) {
		CbmpDecode(img, frame, dst_bits, shape->bits.stride, DECODE_TRANSPARENT, false);
		CanvasUnlock(shape);

		return 0;
	}

	struct rect r = RectMake(0, 0, height, width);
	struct canvas temp_canvas;
	memset(&temp_canvas, 0, sizeof(temp_canvas));
	CanvasInitRect(&temp_canvas, &r, 8, 0xFE);

	CanvasLock(&temp_canvas);

	uint8_t *temp_bits = temp_canvas.bits.bits;
	CbmpDecode(img, frame, temp_bits, temp_canvas.bits.stride, DECODE_OPAQUE, false);

	struct point at = PointMake(shape_bounds.left, shape_bounds.top);
	CANVAS_OPS(shape)->BlitImage(shape, &temp_canvas, &shape_bounds, &at, NULL, false);

	CanvasUnlock(shape);
	CanvasUnlock(&temp_canvas);

	canvas_destroy((struct object *) &temp_canvas);

	return 0;
}

int32_t CanvasDrawFrameAt(struct canvas *canvas, const struct cbmp *img, int32_t frame, struct rect *dirty,
	int32_t x, int32_t y)
{
	dirty->top = dirty->left = dirty->bottom = dirty->right = 0;

	if (!img)
		return 1;

	if (frame < 0 || CbmpFrameCount(img) <= frame)
		return 2;

	int32_t width, height;
	CbmpFrameSize(img, frame, &width, &height);

	int32_t tiles_across = (width + 7) / 8;
	int32_t tiles_down = (height + 7) / 8;
	uint8_t *buf = MemAlloc((size_t) tiles_across * 8 * tiles_down * 8, 1);
	uint8_t *drawn = MemAlloc((size_t) tiles_across * tiles_down, 1);

	CbmpDecodeSkip(img, frame, buf, tiles_across * 8, drawn);

	CanvasLock(canvas);

	uint8_t *dst_bits = canvas->bits.bits;
	int32_t stride = canvas->bits.stride;
	int32_t canvas_w = canvas->bits.rect.right - canvas->bits.rect.left;
	int32_t canvas_h = canvas->bits.rect.bottom - canvas->bits.rect.top;
	bool bottom_up = false;

	if (canvas->backing_dc && canvas->backing_dc->format == 1) {
		bottom_up = true;
		canvas_h = canvas->backing_dc->bitmap->bmHeight;
	}

	if (dst_bits) {
		bool any = false;

		for (int32_t tx = 0; tx < tiles_across; tx++) {
			for (int32_t ty = 0; ty < tiles_down; ty++) {
				if (!drawn[tx * tiles_down + ty])
					continue;

				int32_t left = x + tx * 8;
				int32_t right = left + 8;
				int32_t top = y + ty * 8;
				int32_t bottom = top + 8;

				if (right > x + width)
					right = x + width;

				if (bottom > y + height)
					bottom = y + height;

				if (left < 0)
					left = 0;

				if (top < 0)
					top = 0;

				if (right > canvas_w)
					right = canvas_w;

				if (bottom > canvas_h)
					bottom = canvas_h;

				if (left >= right || top >= bottom)
					continue;

				for (int32_t row = top; row < bottom; row++) {
					uint8_t *dst_row = dst_bits + (size_t) (bottom_up ? canvas_h - 1 - row : row) * stride;
					memcpy(dst_row + left, buf + (size_t) (row - y) * tiles_across * 8 + (left - x),
						(size_t) (right - left));
				}

				if (!any || top < dirty->top)
					dirty->top = top;

				if (!any || left < dirty->left)
					dirty->left = left;

				if (!any || bottom > dirty->bottom)
					dirty->bottom = bottom;

				if (!any || right > dirty->right)
					dirty->right = right;

				any = true;
			}
		}
	}

	CanvasUnlock(canvas);

	MemFree(buf);
	MemFree(drawn);

	return 0;
}
