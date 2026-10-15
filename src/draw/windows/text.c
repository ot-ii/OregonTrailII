#include "../text.h"

#include <stddef.h>

#include <windows.h>

#include "../../base/cstr.h"
#include "../../base/memory.h"
#include "../screendc.h"
#include "../../data/strrsrc.h"

const struct text_ops TEXT_OPS_TABLE = {
	{
		{
			{text_destroy},
			label_draw_brush20,
			shape_draw_brush12,
			shape_draw_outline,
			shape_draw_invert,
			text_draw,
			shape_get_rect_by_val,
			shape_set_rect,
			label_make_region,
		},
		text_set_source,
		text_set_content,
	},
	text_set_linespacing,
	text_set_tabwidth,
};

void TextInitStr(struct text *self, struct rect *bounds, struct cstr *content, int32_t font, int32_t h_align,
	int32_t v_align, int32_t linespacing, int32_t tabwidth, int32_t pen_color, int32_t brush_color,
	int32_t alt_brush_color, int32_t pen_width, int32_t bg_mode)
{
	LabelInitStr(&self->base, bounds, NULL, font, h_align, v_align, pen_color, brush_color, alt_brush_color,
		pen_width, bg_mode);
	self->base.base.base.ops = &TEXT_OPS_TABLE.base.base.base;

	self->content = StrRsrcCreate();
	self->linespacing = linespacing;
	self->tabwidth = tabwidth;
	self->paged = false;
	self->page = 0;
	self->page_count = 1;

	if (content) {
		if (CStrLength(content) > 0)
			StrRsrcAppend(self->content, CStrGet(content));

		CStrDestroy(&content);
	}
}

void TextInitRsrc(struct text *self, struct rect *bounds, struct strrsrc *content, int32_t font, int32_t h_align,
	int32_t v_align, int32_t linespacing, int32_t tabwidth, int32_t pen_color, int32_t brush_color,
	int32_t alt_brush_color, int32_t pen_width, int32_t bg_mode)
{
	LabelInitRsrc(&self->base, bounds, NULL, font, h_align, v_align, pen_color, brush_color, alt_brush_color,
		pen_width, bg_mode);
	self->base.base.base.ops = &TEXT_OPS_TABLE.base.base.base;

	self->content = content;
	self->linespacing = linespacing;
	self->tabwidth = tabwidth;
	self->paged = false;
	self->page = 0;
	self->page_count = 1;
}

struct text *TextCreateStr(struct rect *bounds, struct cstr *content, int32_t font, int32_t h_align,
	int32_t v_align, int32_t linespacing, int32_t tabwidth, int32_t pen_color, int32_t brush_color,
	int32_t alt_brush_color, int32_t pen_width, int32_t bg_mode)
{
	struct text *ctx = MemAlloc(1, sizeof(struct text));
	TextInitStr(ctx, bounds, content, font, h_align, v_align, linespacing, tabwidth, pen_color, brush_color,
		alt_brush_color, pen_width, bg_mode);

	return ctx;
}

struct text *TextCreateRsrc(struct rect *bounds, struct strrsrc *content, int32_t font, int32_t h_align,
	int32_t v_align, int32_t linespacing, int32_t tabwidth, int32_t pen_color, int32_t brush_color,
	int32_t alt_brush_color, int32_t pen_width, int32_t bg_mode)
{
	struct text *ctx = MemAlloc(1, sizeof(struct text));
	TextInitRsrc(ctx, bounds, content, font, h_align, v_align, linespacing, tabwidth, pen_color, brush_color,
		alt_brush_color, pen_width, bg_mode);

	return ctx;
}

void TextDestroy(struct text **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void text_destroy(struct object *self)
{
	struct text *t = (struct text *) self;

	if (t->content)
		StrRsrcDestroy(&t->content);

	self->ops = &LABEL_OPS_TABLE.base.base;
	label_destroy(self);
}

void text_draw(struct shape *self, struct screen_dc *dc)
{
	TextWrapLines((struct text *) self, dc);
	label_draw(self, dc);
}

void text_set_content(struct label *self, struct cstr *content)
{
	struct text *t = (struct text *) self;

	if (t->content)
		StrRsrcDestroy(&t->content);

	t->content = StrRsrcCreate();

	if (content) {
		if (CStrLength(content) > 0)
			StrRsrcAppend(t->content, CStrGet(content));
	}

	CStrDestroy(&content);
}

void text_set_source(struct label *self, struct strrsrc *src)
{
	struct text *t = (struct text *) self;

	if (t->content)
		StrRsrcDestroy(&t->content);

	t->content = src;
}

void text_set_linespacing(struct text *self, int32_t v)
{
	if (v > 0) {
		self->linespacing = v;

	} else {
		self->linespacing = 0;
	}
}

void text_set_tabwidth(struct text *self, int32_t v)
{
	if (v > 0) {
		self->tabwidth = v;

	} else {
		self->tabwidth = 0;
	}
}

int32_t TextGetFirstOverflow(struct text *self)
{
	return self->first_overflow;
}

static int32_t text_height(struct text *self)
{
	struct shape *shape = &self->base.base;
	struct rect r = SHAPE_OPS(shape)->GetRectByVal(shape);

	return r.bottom - r.top;
}

static int32_t text_width(struct text *self)
{
	struct shape *shape = &self->base.base;
	struct rect r = SHAPE_OPS(shape)->GetRectByVal(shape);

	return r.right - r.left;
}

void TextWrapLines(struct text *self, struct screen_dc *dc)
{
	struct cstr *line = CStrCreate();
	struct cstr *word = CStrCreate();
	int32_t metrics[4];
	SIZE extent;

	HDC hdc = dc->hdc;
	ScreenDcSelectFont(dc, self->base.font);
	ScreenDcGetTextMetrics(dc, metrics, self->base.font);
	struct strrsrc *lines = StrRsrcCreate();
	int32_t char_count = 0;
	int32_t line_width = char_count;
	self->first_overflow = -1;
	int32_t y_pos = metrics[3];
	GetTextExtentPointA(hdc, " ", 1, &extent);
	int32_t space_width = extent.cx;
	int32_t para_count = StrRsrcCount(self->content);

	for (int32_t para_index = 0; para_index < para_count; para_index++) {
		struct cstr *paragraph = StrRsrcGetCStr(self->content, para_index);

		if (!paragraph)
			paragraph = CStrCreate();

		CStrAssignBuf(line, "");
		int32_t line_chars = 0;
		line_width = line_chars;
		bool have_word = CStrGetWord2(paragraph, 0, word, 1);

		while (have_word) {
			const char *word_chars = CStrGet(word);
			int32_t word_len = CStrLength(word);

			if (word_len == 1 && (uint8_t) *word_chars == 9) {
				CStrAssignBuf(word, "");

				for (int32_t tab_index = 0; self->tabwidth > tab_index; ++tab_index)
					CStrAppendBuf(word, " ");

				word_chars = CStrGet(word);
			}

			word_len = CStrLength(word);

			if (word_len == 1 && (uint8_t) *word_chars < 0x20) {
				StrRsrcAppend(lines, CStrGet(line));
				CStrAssignBuf(line, "");
				y_pos += metrics[3];
				char_count += line_chars;
				line_chars = 0;
				line_width = line_chars;

				if (self->first_overflow == -1) {
					if (text_height(self) < y_pos)
						self->first_overflow = char_count;
				}

			} else {
				GetTextExtentPointA(hdc, word_chars, CStrLength(word), &extent);
				int32_t word_width = extent.cx;

				if (text_width(self) > space_width + line_width + word_width) {
					if (line_width > 0)
						CStrAppendBuf(line, " ");

					CStrAppendStr(line, word);
					line_width = space_width + line_width + word_width;
					line_chars = line_chars + word_len + 1;

				} else {
					y_pos += metrics[3];
					char_count += line_chars;

					if (self->first_overflow == -1) {
						if (text_height(self) < y_pos)
							self->first_overflow = char_count;
					}

					StrRsrcAppend(lines, CStrGet(line));
					CStrAssign(line, word);
					line_width = word_width;
					line_chars = word_len;
				}
			}

			have_word = CStrGetWord2(paragraph, -2, word, 1);
		}

		if (CStrLength(line) > 0)
			StrRsrcAppend(lines, CStrGet(line));

		if (self->linespacing && para_count - 1 > para_index) {
			CStrAssignBuf(line, " ");
			StrRsrcAppend(lines, CStrGet(line));
		}

		CStrDestroy(&paragraph);
	}

	{
		int32_t per_page = metrics[3] > 0 ? text_height(self) / metrics[3] : 0;
		int32_t total = StrRsrcCount(lines);

		if (per_page < 1)
			per_page = 1;

		self->page_count = total > 0 ? (total + per_page - 1) / per_page : 1;

		if (self->page >= self->page_count)
			self->page = self->page_count - 1;

		if (self->page < 0)
			self->page = 0;

		if (self->paged && self->page_count > 1) {
			struct strrsrc *shown = StrRsrcCreate();
			int32_t end = (self->page + 1) * per_page;

			for (int32_t i = self->page * per_page; i < total && i < end; ++i)
				StrRsrcAppend(shown, StrRsrcGetAt(lines, i));

			StrRsrcDestroy(&lines);
			lines = shown;
		}
	}

	label_set_source(&self->base, lines);

	CStrDestroy(&word);
	CStrDestroy(&line);
}
