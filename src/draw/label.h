#pragma once

#include <stdint.h>

#include "../base/geom.h"
#include "shape.h"

struct cstr;
struct label;
struct strrsrc;

struct label_ops {
	struct shape_ops base;
	void (*SetSource)(struct label *self, struct strrsrc *src);
	void (*SetContent)(struct label *self, struct cstr *content);
};

struct label {
	struct shape base;
	struct strrsrc *text;
	int32_t h_align;
	int32_t v_align;
	int32_t bg_mode;
	int32_t font;
};

#define LABEL_OPS(p) ((const struct label_ops *) ((const struct object *) (p))->ops)

extern const struct label_ops LABEL_OPS_TABLE;

void LabelInitStr(struct label *self, struct rect *bounds, struct cstr *content, int32_t font, int32_t h_align,
	int32_t v_align, int32_t pen_color, int32_t brush_color, int32_t alt_brush_color, int32_t pen_width,
	int32_t bg_mode);
void LabelInitRsrc(struct label *self, struct rect *bounds, struct strrsrc *content, int32_t font, int32_t h_align,
	int32_t v_align, int32_t pen_color, int32_t brush_color, int32_t alt_brush_color, int32_t pen_width,
	int32_t bg_mode);
struct label *LabelCreateStr(struct rect *bounds, struct cstr *content, int32_t font, int32_t h_align,
	int32_t v_align, int32_t pen_color, int32_t brush_color, int32_t alt_brush_color, int32_t pen_width,
	int32_t bg_mode);
struct label *LabelCreateRsrc(struct rect *bounds, struct strrsrc *content, int32_t font, int32_t h_align,
	int32_t v_align, int32_t pen_color, int32_t brush_color, int32_t alt_brush_color, int32_t pen_width,
	int32_t bg_mode);
void LabelDestroy(struct label **ctx_mod);
void label_destroy(struct object *self);

void label_draw_brush20(struct shape *self, struct screen_dc *dc);
void label_draw(struct shape *self, struct screen_dc *dc);
struct region *label_make_region(struct shape *self);

void label_set_source(struct label *self, struct strrsrc *src);
void label_set_content(struct label *self, struct cstr *content);

void LabelInitText(struct label *self, struct strrsrc *text, int32_t font, int32_t h_align, int32_t v_align,
	int32_t bg_mode);
int32_t LabelMeasureWidth(struct label *self, struct screen_dc *dc, int32_t index);
void LabelDrawLines(struct label *self, struct screen_dc *dc);
