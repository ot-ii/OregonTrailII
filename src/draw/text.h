#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "label.h"

struct cstr;
struct strrsrc;
struct text;

struct text_ops {
	struct label_ops base;
	void (*SetLinespacing)(struct text *self, int32_t v);
	void (*SetTabwidth)(struct text *self, int32_t v);
};

struct text {
	struct label base;
	struct strrsrc *content;
	int32_t linespacing;
	int32_t tabwidth;
	int32_t first_overflow;
	bool paged;
	int32_t page;
	int32_t page_count;
};

#define TEXT_OPS(p) ((const struct text_ops *) ((const struct object *) (p))->ops)

extern const struct text_ops TEXT_OPS_TABLE;

void TextInitStr(struct text *self, struct rect *bounds, struct cstr *content, int32_t font, int32_t h_align,
	int32_t v_align, int32_t linespacing, int32_t tabwidth, int32_t pen_color, int32_t brush_color,
	int32_t alt_brush_color, int32_t pen_width, int32_t bg_mode);
void TextInitRsrc(struct text *self, struct rect *bounds, struct strrsrc *content, int32_t font, int32_t h_align,
	int32_t v_align, int32_t linespacing, int32_t tabwidth, int32_t pen_color, int32_t brush_color,
	int32_t alt_brush_color, int32_t pen_width, int32_t bg_mode);
struct text *TextCreateStr(struct rect *bounds, struct cstr *content, int32_t font, int32_t h_align,
	int32_t v_align, int32_t linespacing, int32_t tabwidth, int32_t pen_color, int32_t brush_color,
	int32_t alt_brush_color, int32_t pen_width, int32_t bg_mode);
struct text *TextCreateRsrc(struct rect *bounds, struct strrsrc *content, int32_t font, int32_t h_align,
	int32_t v_align, int32_t linespacing, int32_t tabwidth, int32_t pen_color, int32_t brush_color,
	int32_t alt_brush_color, int32_t pen_width, int32_t bg_mode);
void TextDestroy(struct text **ctx_mod);
void text_destroy(struct object *self);

void text_draw(struct shape *self, struct screen_dc *dc);
void text_set_source(struct label *self, struct strrsrc *src);
void text_set_content(struct label *self, struct cstr *content);

void text_set_linespacing(struct text *self, int32_t v);
void text_set_tabwidth(struct text *self, int32_t v);

void TextWrapLines(struct text *self, struct screen_dc *dc);
int32_t TextGetFirstOverflow(struct text *self);
