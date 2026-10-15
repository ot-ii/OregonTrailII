#pragma once

#include <stdint.h>

#include "clipview.h"
#include "../base/geom.h"

struct cstr;
struct strrsrc;
struct text;

struct text_view_ops {
	struct clip_view_ops base;
};

struct text_view {
	struct clip_view base;
};

#define TEXT_VIEW_OPS(p) ((const struct text_view_ops *) ((const struct object *) (p))->ops)

extern const struct text_view_ops TEXT_VIEW_OPS_TABLE;

void TextViewInit(struct text_view *self, int32_t id, const struct rect *rect, struct cstr *src, int32_t font_id,
	int32_t h_align, int32_t v_align, int32_t linespacing, int32_t tabwidth, int32_t pen, int32_t brush, int32_t alt,
	int32_t pen_width, int32_t bg_mode);
void TextViewInitStrs(struct text_view *self, int32_t id, const struct rect *rect, struct strrsrc *rsrc_src,
	int32_t font_id, int32_t h_align, int32_t v_align, int32_t linespacing, int32_t tabwidth, int32_t pen,
	int32_t brush, int32_t alt, int32_t pen_width, int32_t bg_mode);
void TextViewInitText(struct text_view *self, int32_t id, const struct rect *rect, struct text *text);
struct text_view *TextViewCreate(int32_t id, const struct rect *rect, struct cstr *src, int32_t font_id,
	int32_t h_align, int32_t v_align, int32_t linespacing, int32_t tabwidth, int32_t pen, int32_t brush, int32_t alt,
	int32_t pen_width, int32_t bg_mode);
struct text_view *TextViewCreateStrs(int32_t id, const struct rect *rect, struct strrsrc *rsrc_src, int32_t font_id,
	int32_t h_align, int32_t v_align, int32_t linespacing, int32_t tabwidth, int32_t pen, int32_t brush, int32_t alt,
	int32_t pen_width, int32_t bg_mode);
struct text_view *TextViewCreateText(int32_t id, const struct rect *rect, struct text *text);
void TextViewDestroy(struct text_view **ctx_mod);
void text_view_destroy(struct object *self);

void TextViewSetSourceObj(struct text_view *self, struct text *text);
void TextViewSetSource(struct text_view *self, struct cstr *src);
void TextViewSetText(struct text_view *self, struct strrsrc *text);

struct view *CreateTextView(struct ctrl_desc *desc);
