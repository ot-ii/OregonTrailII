#include "textlist.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "../draw/canvas.h"
#include "../base/cstr.h"
#include "../draw/label.h"
#include "../base/memory.h"
#include "../draw/offscreendc.h"
#include "../draw/palette.h"
#include "../draw/screendc.h"
#include "../draw/shape.h"
#include "../data/strrsrc.h"

const struct text_list_ops TEXT_LIST_OPS_TABLE = {
	{
		{
			{text_list_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			view_on_event,
			view_on_idle,
			text_list_on_idle_spare,
		},
		view_add_child,
		view_get_bounds,
		view_get_clip_region,
		view_hide,
		view_remove_child,
		view_remove_from_parent,
		view_set_cursor_id,
		view_set_device,
		view_show,
		text_list_draw_children,
		view_move_children,
	},
};

void TextListInit(struct text_list *self, int32_t id, const struct rect *rect, struct strrsrc *text_src,
	int32_t scroll_step, int32_t idle_event_id)
{
	ViewInit(&self->base, EVCLS_SCROLLVIEW, id, rect);
	self->base.base.base.ops = &TEXT_LIST_OPS_TABLE.base.base.base;

	self->text_src = text_src;
	self->scroll_step = scroll_step;
	self->bounds = *rect;
	self->bounds.right -= self->bounds.left;
	self->bounds.bottom -= self->bounds.top;
	self->bounds.top = 0;
	self->bounds.left = self->bounds.top;

	self->maskdc = OffscreenDcCreateRect(&self->bounds, g_palette, 8);

	self->idle_event_id = idle_event_id;

	int32_t handler_id = CMD_SCROLLVIEW_CTRL;
	EvtHandlerBindHandler(&self->base.base, 1, &handler_id);
	EvtHandlerAddIdleHandler(&self->base.base, false, self->idle_event_id);

	for (int32_t row_index = 0; row_index < 80; ++row_index) {
		self->row_y[row_index] = -999;
		self->row_str_index[row_index] = 0;
		self->row_data[row_index] = NULL;
	}

	self->row_count = 0;
}

struct text_list *TextListCreate(int32_t id, const struct rect *rect, struct strrsrc *text_src, int32_t scroll_step,
	int32_t idle_event_id)
{
	struct text_list *self = MemAlloc(1, sizeof(struct text_list));
	TextListInit(self, id, rect, text_src, scroll_step, idle_event_id);

	return self;
}

void TextListDestroy(struct text_list **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void text_list_destroy(struct object *obj)
{
	struct text_list *self = (struct text_list *) obj;

	for (int32_t row_index = 0; row_index < 80; ++row_index) {
		if (self->row_data[row_index])
			CStrDestroy(&self->row_data[row_index]);
	}

	if (self->maskdc)
		ObjectDelete(self->maskdc);

	if (self->text_src)
		StrRsrcDestroy(&self->text_src);

	obj->ops = &VIEW_OPS_TABLE.base.base;
	view_destroy(obj);
}

static void blit_mask(struct text_list *self, struct screen_dc *dc, struct rect *src_rect, struct rect *dst_rect)
{
	struct canvas source;
	memset(&source, 0, sizeof(source));
	source.bits.bits = (uint8_t *) self->maskdc->base.hdc;

	SCREEN_DC_OPS(dc)->BitBlt(dc, &source, src_rect, dst_rect, 0, 0);
}

static void split_line(struct cstr *line_str, int32_t *font)
{
	const char *line_text = CStrGet(line_str);
	sscanf(line_text, "%d", font);

	int32_t tab_pos = 0;

	while (line_text[tab_pos++] != 9)
		;

	CStrSubstr(line_str, tab_pos + 1, 999);
}

void text_list_draw_children(struct view *view, struct screen_dc *dc, const struct rect *bounds)
{
	struct text_list *self = (struct text_list *) view;

	struct rect mask_rect = *bounds;
	struct rect dest_rect = *bounds;
	int32_t font_id = 0;
	int32_t metrics[4];

	struct rect row_rect = RectMake(0, 10, 0, view->rect_b.right - view->rect_b.left);
	struct label *label = LabelCreateStr(&row_rect, NULL, 0, 0, 2, 0, 0, 255, 0, 1);

	RectIntersect(&mask_rect, &view->rect_b, &mask_rect);
	RectIntersect(&dest_rect, &view->rect_b, &dest_rect);
	RectOffset(&mask_rect, -view->rect_b.left, -view->rect_b.top);

	struct screen_dc *mask = &self->maskdc->base;
	SCREEN_DC_OPS(mask)->SetBkColor(mask, 10);
	ScreenDcFillRect(mask, &mask_rect, 0);

	if (!self->row_count) {
		int32_t fill_height = view->rect_b.bottom - view->rect_b.top + 120;
		int32_t next_y = 0;
		int32_t row_index = 0;
		int32_t row_height;

		do {
			self->row_str_index[row_index] = row_index;
			struct cstr *line_str = StrRsrcGetCStr(self->text_src, row_index);

			if (CStrLength(line_str) > 1) {
				split_line(line_str, &font_id);
				ScreenDcGetTextMetrics(dc, metrics, font_id);
				self->row_h[row_index] = font_id;
				self->row_data[row_index] = line_str;
				row_height = metrics[3] + 1;

			} else {
				self->row_data[row_index] = NULL;
				row_height = 10;
				CStrDestroy(&line_str);
			}

			next_y += row_height;
			self->row_y[row_index] = next_y;
			++row_index;
		} while (next_y < fill_height);

		self->row_count = row_index;
		self->row_str_index[self->row_count] = row_index;
	}

	for (int32_t row_index = 0; self->row_count > row_index; ++row_index) {
		row_rect.bottom = self->row_y[row_index];
		row_rect.top = self->row_y[row_index] - 50;

		struct rect hit;

		if (self->row_data[row_index] && RectIntersect(&row_rect, &mask_rect, &hit)) {
			struct cstr *line_str = CStrCreateCopy(self->row_data[row_index]);
			label->font = self->row_h[row_index];
			LABEL_OPS(label)->SetContent(label, line_str);
			SHAPE_OPS(label)->SetRect(&label->base, &row_rect);
			SHAPE_OPS(label)->Draw(&label->base, mask);
		}
	}

	blit_mask(self, dc, &mask_rect, &dest_rect);
	ObjectDelete(label);
}

void text_list_on_idle_spare(struct evt_handler *handler, void *task)
{
	struct text_list *self = (struct text_list *) handler;
	struct view *view = &self->base;

	int32_t font_id = 0;
	int32_t metrics[4];
	int32_t row_height;

	struct screen_dc *win_dc = ViewGetWinDc(view);

	if (win_dc) {
		for (int32_t row_index = 0; row_index < self->row_count; ++row_index)
			self->row_y[row_index] -= self->scroll_step;

		struct rect scroll_rect = view->rect_b;
		ScreenDcScroll(win_dc, &scroll_rect, 0, -self->scroll_step, 0);
		scroll_rect.top = scroll_rect.bottom - self->scroll_step;
		VIEW_OPS(view)->DrawChildren(view, win_dc, &scroll_rect);

		if (self->row_y[1] <= 0) {
			int32_t last_row = self->row_count - 1;
			int32_t next_index = self->row_str_index[last_row] + 1;

			if (self->row_data[0])
				CStrDestroy(&self->row_data[0]);

			for (int32_t row_index = 0; row_index < last_row; ++row_index) {
				self->row_y[row_index] = self->row_y[row_index + 1];
				self->row_str_index[row_index] = self->row_str_index[row_index + 1];
				self->row_data[row_index] = self->row_data[row_index + 1];
				self->row_h[row_index] = self->row_h[row_index + 1];
			}

			if (StrRsrcCount(self->text_src) <= next_index)
				next_index = 0;

			self->row_str_index[last_row] = next_index;
			struct cstr *line_str = StrRsrcGetCStr(self->text_src, next_index);

			if (CStrLength(line_str) > 1) {
				split_line(line_str, &font_id);
				ScreenDcGetTextMetrics(win_dc, metrics, font_id);
				self->row_h[last_row] = font_id;
				row_height = metrics[3];
				self->row_data[last_row] = line_str;

			} else {
				CStrDestroy(&line_str);
				row_height = 10;
				self->row_data[last_row] = NULL;
			}

			self->row_y[last_row] = row_height + self->row_y[last_row - 1];
		}
	}
}
