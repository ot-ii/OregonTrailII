#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "shape.h"

struct canvas;
struct cbmp;
struct palette;
struct screen_dc;

struct canvas_bits {
	uint8_t *bits;
	int32_t stride;
	struct rect rect;
	int32_t valid;
	int32_t depth;
};

struct backing_bitmap {
	int32_t bmType;
	int32_t bmWidth;
	int32_t bmHeight;
	int32_t bmWidthBytes;
	int32_t bmPlanes;
	int32_t bmBitsPixel;
	void *bmBits;
};

struct backing_dc {
	uint8_t *bits;
	struct backing_bitmap *bitmap;
	int32_t format;
};

struct canvas_ops {
	struct shape_ops base;
	void (*StretchBlit)(struct canvas *self, struct canvas *src_canvas, struct rect *src_rect, struct rect *dst_rect,
		int32_t mode, struct rect *clip_rect);
	void (*AlphaBlend)(struct canvas *self, struct canvas *src_canvas, struct canvas *alpha_canvas,
		struct point *pos, struct rect *blend_rect, int32_t unused, struct rect *clip_rect, struct palette *palette);
	void (*DrawToDevice)(struct canvas *self, struct screen_dc *dc, struct rect *src_rect, struct rect *dst_rect,
		int32_t mode, struct rect *clip_rect);
	void (*DrawAt)(struct canvas *self, struct screen_dc *dc, struct point *pos, int32_t mode);
	void (*BlitMasked)(struct canvas *self, struct canvas *dst_canvas, struct canvas *src_canvas,
		struct rect *dst_rect, struct point *pos, struct rect *clip_rect, int32_t threshold, bool flip_x);
	void (*BlitImage)(struct canvas *self, struct canvas *dst_canvas, struct rect *src_rect, struct point *pos,
		struct rect *clip_rect, bool flip_x);
	void (*SetupBuffer)(struct canvas *self, struct rect *rc, struct rect *shape_rect, struct rect *bounds_rect,
		int32_t depth, uint8_t clear_color, int32_t blit_mode);
};

struct canvas {
	struct shape base;
	struct canvas_bits bits;
	struct backing_dc *backing_dc;
	uint8_t *dib;
	int32_t dib_size;
	int32_t dib_lock_count;
	bool dib_purgeable;
	bool dib_in_lru;
	struct canvas *dib_lru_prev;
	struct canvas *dib_lru_next;
	struct rect bounds;
	int32_t blit_mode;
};

#define CANVAS_OPS(p) ((const struct canvas_ops *) ((const struct object *) (p))->ops)

extern const struct canvas_ops CANVAS_OPS_TABLE;

void CanvasInit(struct canvas *self);
void CanvasInitRect(struct canvas *self, struct rect *rc, int32_t depth, uint8_t clear_color);
void CanvasInitAt(struct canvas *self, struct rect *rc, struct point *pt, int32_t depth, uint8_t clear_color,
	int32_t blit_mode);
void CanvasInitFull(struct canvas *self, struct rect *rc, struct rect *shape_rect, struct rect *bounds_rect,
	int32_t depth, uint8_t clear_color, int32_t blit_mode, int32_t pen_color, int32_t brush_color,
	int32_t alt_brush_color, int32_t pen_width, int32_t pen_style);
struct canvas *CanvasCreate(void);
struct canvas *CanvasCreateRect(struct rect *rc, int32_t depth, uint8_t clear_color);
struct canvas *CanvasCreateAt(struct rect *rc, struct point *pt, int32_t depth, uint8_t clear_color,
	int32_t blit_mode);
struct canvas *CanvasCreateFull(struct rect *rc, struct rect *shape_rect, struct rect *bounds_rect, int32_t depth,
	uint8_t clear_color, int32_t blit_mode, int32_t pen_color, int32_t brush_color, int32_t alt_brush_color,
	int32_t pen_width, int32_t pen_style);
void CanvasDestroy(struct canvas **ctx_mod);
void canvas_destroy(struct object *self);

void canvas_draw_brush20(struct shape *self, struct screen_dc *dc);
void canvas_draw(struct shape *self, struct screen_dc *dc);

void canvas_stretch_blit(struct canvas *self, struct canvas *src_canvas, struct rect *src_rect,
	struct rect *dst_rect, int32_t mode, struct rect *clip_rect);
void canvas_alpha_blend(struct canvas *self, struct canvas *src_canvas, struct canvas *alpha_canvas,
	struct point *pos, struct rect *blend_rect, int32_t unused, struct rect *clip_rect, struct palette *palette);
void canvas_draw_to_device(struct canvas *self, struct screen_dc *dc, struct rect *src_rect, struct rect *dst_rect,
	int32_t mode, struct rect *clip_rect);
void canvas_draw_at(struct canvas *self, struct screen_dc *dc, struct point *pos, int32_t mode);
void canvas_blit_masked(struct canvas *self, struct canvas *dst_canvas, struct canvas *src_canvas,
	struct rect *dst_rect, struct point *pos, struct rect *clip_rect, int32_t threshold, bool flip_x);
void canvas_blit_image(struct canvas *self, struct canvas *dst_canvas, struct rect *src_rect, struct point *pos,
	struct rect *clip_rect, bool flip_x);
void canvas_setup_buffer(struct canvas *self, struct rect *rc, struct rect *shape_rect, struct rect *bounds_rect,
	int32_t depth, uint8_t clear_color, int32_t blit_mode);
struct rect CanvasGetBounds(struct canvas *self);
void CanvasFill(struct canvas *self, uint8_t color);
uint8_t CanvasGetPixel(struct canvas *self, int32_t x, int32_t y);
void CanvasFillRect(struct canvas *self, struct rect *rect, uint8_t pat);
void CanvasLock(struct canvas *self);
void CanvasUnlock(struct canvas *self);
void CanvasDibLocked(struct canvas *self);
void CanvasDibUnlocked(struct canvas *self);

void CanvasSetPurgeable(struct canvas *self, bool flag);
bool CanvasDibInvalid(struct canvas *self);

int32_t CanvasDrawFrame(struct canvas *canvas, const struct cbmp *img, int32_t frame);
int32_t CanvasDrawFrameMasked(struct canvas *shape, const struct cbmp *img, int32_t frame);
int32_t CanvasDrawFrameAt(struct canvas *canvas, const struct cbmp *img, int32_t frame, struct rect *dirty,
	int32_t x, int32_t y);

extern bool g_use_dib_sections;
