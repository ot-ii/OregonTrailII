#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "../base/object.h"

struct canvas;
struct palette;
struct region;
struct screen_dc;

struct screen_dc_ops {
	struct object_ops base;
	void (*BitBlt)(struct screen_dc *self, struct canvas *src, struct rect *src_rect, struct rect *dst_rect,
		int32_t mode, int32_t unused);
	void (*BlitCanvas)(struct screen_dc *self, struct canvas *image, struct rect *src_rect, struct rect *dst_rect,
		int32_t mode, struct rect *clip);
	void (*DrawImage)(struct screen_dc *self, struct canvas *image, struct point *pos, int32_t mode);
	struct canvas *(*CaptureRect)(struct screen_dc *self, struct rect *src_rect);
	struct point (*GetCursorPoint)(struct screen_dc *self);
	bool (*IsScreen)(struct screen_dc *self);
	void (*SetBkColor)(struct screen_dc *self, int32_t color);
	void (*SetClipRegion)(struct screen_dc *self, struct region *rgn);
	void (*SetClipRect)(struct screen_dc *self, struct rect *rect);
	void (*SetColor)(struct screen_dc *self, int32_t color);
	void (*Sync)(struct screen_dc *self);
};

struct screen_dc {
	struct object base;
	struct rect rc;
	struct palette *palette_obj;
	int32_t is_valid;
	uint8_t *bmi_buf;
	int32_t bmi_buf_size;
	void *hpal;
	bool high_color;
	void *hdc;
	void *pen;
	int32_t pen_color;
	int32_t pen_dashed;
	void *brush;
	int32_t brush_color;
	int32_t bk_color;
	int32_t text_color;
	int32_t draw_mode;
	int32_t pen_width;
	int32_t pen_width2;
	int32_t font_id;
};

#define SCREEN_DC_OPS(p) ((const struct screen_dc_ops *) ((const struct object *) (p))->ops)

extern const struct screen_dc_ops SCREEN_DC_OPS_TABLE;

void ScreenDcInit(struct screen_dc *self);
struct screen_dc *ScreenDcCreate(void);
void ScreenDcDestroy(struct screen_dc **ctx_mod);
void screen_dc_destroy(struct object *self);

void screen_dc_bit_blt(struct screen_dc *self, struct canvas *src, struct rect *src_rect, struct rect *dst_rect,
	int32_t mode, int32_t unused);
void screen_dc_blit_canvas(struct screen_dc *self, struct canvas *image, struct rect *src_rect,
	struct rect *dst_rect, int32_t mode, struct rect *clip);
void screen_dc_draw_image(struct screen_dc *self, struct canvas *image, struct point *pos, int32_t mode);
struct canvas *screen_dc_capture_rect(struct screen_dc *self, struct rect *src_rect);
struct point screen_dc_get_cursor_point(struct screen_dc *self);
bool screen_dc_is_screen(struct screen_dc *self);
void screen_dc_set_bk_color(struct screen_dc *self, int32_t color);
void screen_dc_set_clip_region(struct screen_dc *self, struct region *rgn);
void screen_dc_set_clip_rect(struct screen_dc *self, struct rect *rect);
void screen_dc_set_color(struct screen_dc *self, int32_t color);
void screen_dc_sync(struct screen_dc *self);

void ScreenDcSelectFont(struct screen_dc *self, int32_t font_id);
void ScreenDcGetTextMetrics(struct screen_dc *self, int32_t out[4], int32_t font);
void ScreenDcFillRect(struct screen_dc *self, struct rect *rect, int32_t mode);
void ScreenDcFrameRect(struct screen_dc *self, struct rect *rect, int32_t mode);
void ScreenDcInvertRect(struct screen_dc *self, struct rect *rect, int32_t mode);
void ScreenDcSetBrush(struct screen_dc *self, int32_t color);
void ScreenDcSetPen(struct screen_dc *self, int32_t color, int32_t width, int32_t dashed);
void ScreenDcSetDrawMode(struct screen_dc *self, int32_t mode);
void ScreenDcLinePoints(struct screen_dc *self, struct point *from, struct point *to);
void ScreenDcLineCoords(struct screen_dc *self, int32_t v1, int32_t h1, int32_t v2, int32_t h2);
void ScreenDcSelectPalette(struct screen_dc *self, struct palette *pal);
void ScreenDcScroll(struct screen_dc *self, struct rect *rect, int32_t dx, int32_t dy, int32_t mode);
struct point ScreenDcPointToLocal(struct screen_dc *self, struct point *pt);
struct point ScreenDcPointToScreen(struct screen_dc *self, struct point *pt);
struct rect ScreenDcRectToGlobal(struct screen_dc *self, struct rect *src);
struct rect ScreenDcScreenToClient(struct screen_dc *self, struct rect *r);
void ScreenDcDrawImageIfValid(struct screen_dc *self, struct canvas *image, struct point *pos, int32_t mode);
void ScreenDcInitValid(struct screen_dc *self, void *hdc, struct rect *bounds, struct palette *palette_obj);
void ScreenDcInitDc(struct screen_dc *self, void *dc, struct rect *bounds, struct palette *pal);
struct region *ScreenDcGetClip(struct screen_dc *self);
int32_t ScreenDcGetPenWidth(struct screen_dc *self);
void ScreenDcSetPenWidth(struct screen_dc *self, int32_t w);
void ScreenDcSetPenWidth2(struct screen_dc *self, int32_t w);
void ScreenDcReleaseObjects(struct screen_dc *self);
uint32_t ScreenDcColorToColorref(struct screen_dc *self, int32_t color);
