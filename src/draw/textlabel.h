#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "../base/object.h"

struct cstr;
struct region;
struct screen_dc;
struct text;
struct text_label;

struct text_label_ops {
	struct object_ops base;
	struct rect (*GetBounds)(struct text_label *self);
	void (*SetBounds)(struct text_label *self, struct rect *r);
	void (*DrawInner)(struct text_label *self, struct screen_dc *dc, struct point *offset, int32_t value,
		int32_t draw_mode, int32_t a6);
	struct region *(*MakeRegion)(struct text_label *self, struct point *offset);
	void (*SetText)(struct text_label *self, struct cstr *source);
	void (*Draw)(struct text_label *self, struct screen_dc *dc, struct point *offset, struct rect *src_rect,
		bool brushed, int32_t color_mode);
};

struct text_label {
	struct object base;
	struct text *text;
	struct rect bounds;
	int32_t font_id;
	int32_t color_default;
	int32_t unused_14;
	int32_t pen_color;
	int32_t color_mode1;
	int32_t color_mode2;
	int32_t align;
};

#define TEXT_LABEL_OPS(p) ((const struct text_label_ops *) ((const struct object *) (p))->ops)

extern const struct text_label_ops TEXT_LABEL_OPS_TABLE;

void TextLabelInitRect(struct text_label *self, struct rect *bounds, struct cstr *source, int32_t font_id,
	int32_t align);
void TextLabelInit(struct text_label *self, struct cstr *source, int32_t font_id, int32_t align);
struct text_label *TextLabelCreateRect(struct rect *bounds, struct cstr *source, int32_t font_id, int32_t align);
struct text_label *TextLabelCreate(struct cstr *source, int32_t font_id, int32_t align);
void TextLabelDestroy(struct text_label **ctx_mod);
void text_label_destroy(struct object *self);

struct rect text_label_get_bounds(struct text_label *self);
void text_label_set_bounds(struct text_label *self, struct rect *r);
void text_label_draw_inner(struct text_label *self, struct screen_dc *dc, struct point *offset, int32_t value,
	int32_t draw_mode, int32_t a6);
struct region *text_label_make_region(struct text_label *self, struct point *offset);
void text_label_set_text(struct text_label *self, struct cstr *source);
void text_label_draw(struct text_label *self, struct screen_dc *dc, struct point *offset, struct rect *src_rect,
	bool brushed, int32_t color_mode);
