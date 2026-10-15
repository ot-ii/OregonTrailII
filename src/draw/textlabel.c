#include "textlabel.h"

#include <stddef.h>

#include "../base/memory.h"
#include "region.h"
#include "shape.h"
#include "text.h"

const struct text_label_ops TEXT_LABEL_OPS_TABLE = {
	{text_label_destroy},
	text_label_get_bounds,
	text_label_set_bounds,
	text_label_draw_inner,
	text_label_make_region,
	text_label_set_text,
	text_label_draw,
};

void TextLabelInit(struct text_label *self, struct cstr *source, int32_t font_id, int32_t align)
{
	ObjectInit(&self->base);
	self->base.ops = &TEXT_LABEL_OPS_TABLE.base;
	self->align = align;
	self->bounds = RectMake(0, 0, 0, 0);
	self->text = NULL;
	self->font_id = font_id;
	self->pen_color = 0;
	self->color_default = self->pen_color;
	self->color_mode2 = self->color_default;
	self->color_mode1 = self->color_mode2;
	self->unused_14 = 255;
	text_label_set_text(self, source);
}

void TextLabelInitRect(struct text_label *self, struct rect *bounds, struct cstr *source, int32_t font_id,
	int32_t align)
{
	ObjectInit(&self->base);
	self->base.ops = &TEXT_LABEL_OPS_TABLE.base;
	self->align = align;
	self->bounds = *bounds;
	self->text = NULL;
	self->font_id = font_id;
	self->pen_color = 0;
	self->color_default = self->pen_color;
	self->color_mode2 = self->color_default;
	self->color_mode1 = self->color_mode2;
	self->unused_14 = 255;
	text_label_set_text(self, source);
}

struct text_label *TextLabelCreateRect(struct rect *bounds, struct cstr *source, int32_t font_id, int32_t align)
{
	struct text_label *ctx = MemAlloc(1, sizeof(struct text_label));
	TextLabelInitRect(ctx, bounds, source, font_id, align);

	return ctx;
}

struct text_label *TextLabelCreate(struct cstr *source, int32_t font_id, int32_t align)
{
	struct text_label *ctx = MemAlloc(1, sizeof(struct text_label));
	TextLabelInit(ctx, source, font_id, align);

	return ctx;
}

void TextLabelDestroy(struct text_label **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void text_label_destroy(struct object *self)
{
	struct text_label *tl = (struct text_label *) self;

	if (tl->text)
		ObjectDestroy(&tl->text);

	object_destroy(self);
}

struct rect text_label_get_bounds(struct text_label *self)
{
	return self->bounds;
}

void text_label_draw_inner(struct text_label *self, struct screen_dc *dc, struct point *offset, int32_t value,
	int32_t draw_mode, int32_t a6)
{
	(void) value;
	(void) a6;

	if (self->text) {
		int32_t mode = 0;

		if (draw_mode == 1)
			mode = 2;

		TEXT_LABEL_OPS(self)->Draw(self, dc, offset, &self->bounds, self->align != 0, mode);
	}
}

void text_label_draw(struct text_label *self, struct screen_dc *dc, struct point *offset, struct rect *src_rect,
	bool brushed, int32_t color_mode)
{
	if (self->text) {
		struct shape *text = (struct shape *) self->text;
		struct rect rect = *src_rect;
		rect.left += offset->h;
		rect.right += offset->h;
		rect.top += offset->v;
		rect.bottom += offset->v;
		SHAPE_OPS(text)->SetRect(text, &rect);

		switch (color_mode) {
			case 1:
				text->brushColor = self->color_mode1;
				break;
			case 2:
				text->brushColor = self->color_mode2;
				break;
			default:
				text->brushColor = self->color_default;
				break;
		}

		if (brushed) {
			SHAPE_OPS(text)->DrawBrush20(text, dc);

		} else {
			SHAPE_OPS(text)->Draw(text, dc);
		}
	}
}

struct region *text_label_make_region(struct text_label *self, struct point *offset)
{
	struct rect rect = TEXT_LABEL_OPS(self)->GetBounds(self);
	rect.left += offset->h;
	rect.right += offset->h;
	rect.top += offset->v;
	rect.bottom += offset->v;

	return RegionCreateRect(&rect, 1, 0, 0);
}

void text_label_set_text(struct text_label *self, struct cstr *source)
{
	if (!self->text && source) {
		self->text = TextCreateStr(&self->bounds, source, self->font_id, 1, 1, 0, 0, 0, 0, 255, 1, 1);

	} else {
		if (source) {
			LABEL_OPS(self->text)->SetContent(&self->text->base, source);

		} else if (self->text) {
			ObjectDestroy(&self->text);
		}
	}
}

void text_label_set_bounds(struct text_label *self, struct rect *r)
{
	self->bounds = *r;

	if (self->text)
		SHAPE_OPS(self->text)->SetRect(&self->text->base.base, r);
}
