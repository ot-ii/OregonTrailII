#include "../label.h"

#include <stddef.h>
#include <string.h>

#include <windows.h>

#include "../../base/cstr.h"
#include "../../base/memory.h"
#include "../region.h"
#include "../screendc.h"
#include "../../data/strrsrc.h"

const struct label_ops LABEL_OPS_TABLE = {
	{
		{label_destroy},
		label_draw_brush20,
		shape_draw_brush12,
		shape_draw_outline,
		shape_draw_invert,
		label_draw,
		shape_get_rect_by_val,
		shape_set_rect,
		label_make_region,
	},
	label_set_source,
	label_set_content,
};

void LabelInitStr(struct label *self, struct rect *bounds, struct cstr *content, int32_t font, int32_t h_align,
	int32_t v_align, int32_t pen_color, int32_t brush_color, int32_t alt_brush_color, int32_t pen_width,
	int32_t bg_mode)
{
	ShapeInitRect(&self->base, bounds, pen_color, brush_color, alt_brush_color, pen_width, 1);
	self->base.base.ops = &LABEL_OPS_TABLE.base.base;

	struct strrsrc *text = StrRsrcCreate();

	if (content) {
		if (CStrLength(content) > 0)
			StrRsrcAppend(text, CStrGet(content));

		CStrDestroy(&content);
	}

	LabelInitText(self, text, font, h_align, v_align, bg_mode);
}

void LabelInitRsrc(struct label *self, struct rect *bounds, struct strrsrc *content, int32_t font, int32_t h_align,
	int32_t v_align, int32_t pen_color, int32_t brush_color, int32_t alt_brush_color, int32_t pen_width,
	int32_t bg_mode)
{
	ShapeInitRect(&self->base, bounds, pen_color, brush_color, alt_brush_color, pen_width, 1);
	self->base.base.ops = &LABEL_OPS_TABLE.base.base;

	LabelInitText(self, content, font, h_align, v_align, bg_mode);
}

struct label *LabelCreateStr(struct rect *bounds, struct cstr *content, int32_t font, int32_t h_align,
	int32_t v_align, int32_t pen_color, int32_t brush_color, int32_t alt_brush_color, int32_t pen_width,
	int32_t bg_mode)
{
	struct label *ctx = MemAlloc(1, sizeof(struct label));
	LabelInitStr(ctx, bounds, content, font, h_align, v_align, pen_color, brush_color, alt_brush_color, pen_width,
		bg_mode);

	return ctx;
}

struct label *LabelCreateRsrc(struct rect *bounds, struct strrsrc *content, int32_t font, int32_t h_align,
	int32_t v_align, int32_t pen_color, int32_t brush_color, int32_t alt_brush_color, int32_t pen_width,
	int32_t bg_mode)
{
	struct label *ctx = MemAlloc(1, sizeof(struct label));
	LabelInitRsrc(ctx, bounds, content, font, h_align, v_align, pen_color, brush_color, alt_brush_color,
		pen_width, bg_mode);

	return ctx;
}

void LabelDestroy(struct label **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void label_destroy(struct object *self)
{
	struct label *l = (struct label *) self;

	if (l->text)
		StrRsrcDestroy(&l->text);

	self->ops = &SHAPE_OPS_TABLE.base;
	shape_destroy(self);
}

void label_draw_brush20(struct shape *self, struct screen_dc *dc)
{
	SHAPE_OPS(self)->Draw(self, dc);
	SHAPE_OPS(self)->DrawOutline(self, dc);
}

void LabelInitText(struct label *self, struct strrsrc *text, int32_t font, int32_t h_align, int32_t v_align,
	int32_t bg_mode)
{
	if (text) {
		self->text = text;

	} else {
		self->text = StrRsrcCreate();
	}

	self->font = font;
	self->h_align = h_align;
	self->v_align = v_align;
	self->bg_mode = bg_mode;
}

struct region *label_make_region(struct shape *self)
{
	(void) self;

	return RegionCreate();
}

void label_draw(struct shape *self, struct screen_dc *dc)
{
	SCREEN_DC_OPS(dc)->SetColor(dc, self->brushColor);
	SCREEN_DC_OPS(dc)->SetBkColor(dc, self->altBrushColor);
	LabelDrawLines((struct label *) self, dc);
}

void label_set_content(struct label *self, struct cstr *content)
{
	if (self->text)
		StrRsrcDestroy(&self->text);

	self->text = StrRsrcCreate();

	if (content) {
		if (CStrLength(content) > 0)
			StrRsrcAppend(self->text, CStrGet(content));
	}

	CStrDestroy(&content);
}

void label_set_source(struct label *self, struct strrsrc *src)
{
	if (self->text)
		StrRsrcDestroy(&self->text);

	self->text = src;
}

int32_t LabelMeasureWidth(struct label *self, struct screen_dc *dc, int32_t index)
{
	HDC hdc = dc->hdc;
	SIZE size;

	ScreenDcSelectFont(dc, self->font);

	const char *line = StrRsrcGetAt(self->text, index);

	if (!line)
		line = "";

	int32_t len = (int32_t) strlen(line);
	GetTextExtentPointA(hdc, line, len, &size);

	return size.cx;
}

void LabelDrawLines(struct label *self, struct screen_dc *dc)
{
	int32_t line_count = StrRsrcCount(self->text);

	if (!line_count)
		return;

	struct shape *shape = &self->base;
	struct rect rect = SHAPE_OPS(shape)->GetRectByVal(shape);
	HDC hdc = dc->hdc;
	int32_t metrics[4];
	int32_t height;

	ScreenDcSelectFont(dc, self->font);
	ScreenDcGetTextMetrics(dc, metrics, self->font);
	int32_t line_height = metrics[3];

	switch (self->v_align) {
		case 2:
			height = rect.bottom - line_count * line_height;
			break;
		case 1:
			height = (rect.bottom + rect.top - line_count * line_height) / 2;
			break;
		default:
			height = rect.top;
			break;
	}

	for (int32_t index = 0; line_count > index; ++index) {
		const char *line = StrRsrcGetAt(self->text, index);

		if (!line)
			line = "";

		int32_t len = (int32_t) strlen(line);
		SIZE size;
		GetTextExtentPointA(hdc, line, len, &size);
		int32_t text_width = size.cx;
		int32_t width;

		switch (self->h_align) {
			case 2:
				width = rect.right - text_width;
				break;
			case 1:
				width = (rect.left + rect.right - text_width) / 2;
				break;
			default:
				width = rect.left;
				break;
		}

		if (width < 0)
			width = 0;

		int32_t bk_mode;

		switch (self->bg_mode) {
			case 0:
				bk_mode = OPAQUE;
				break;
			default:
				bk_mode = TRANSPARENT;
				break;
		}

		int32_t old_bk_mode = SetBkMode(hdc, bk_mode);
		TextOutA(hdc, width, height, line, len);
		MoveToEx(hdc, width + size.cx, height, NULL);
		SetBkMode(hdc, old_bk_mode);
		height += line_height;
	}
}
