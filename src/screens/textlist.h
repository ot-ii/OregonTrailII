#pragma once

#include <stdint.h>

#include "../app/event.h"
#include "../base/geom.h"
#include "../ui/view.h"

struct cstr;
struct offscreen_dc;
struct screen_dc;
struct strrsrc;

struct text_list_ops {
	struct view_ops base;
};

struct text_list {
	struct view base;
	struct offscreen_dc *maskdc;
	struct strrsrc *text_src;
	int32_t idle_event_id;
	int32_t row_y[80];
	int32_t row_str_index[80];
	struct cstr *row_data[80];
	int32_t row_h[80];
	int32_t row_count;
	struct rect bounds;
	int32_t scroll_step;
};

#define TEXT_LIST_OPS(p) ((const struct text_list_ops *) ((const struct object *) (p))->ops)

extern const struct text_list_ops TEXT_LIST_OPS_TABLE;

void TextListInit(struct text_list *self, int32_t id, const struct rect *rect, struct strrsrc *text_src,
	int32_t scroll_step, int32_t idle_event_id);
struct text_list *TextListCreate(int32_t id, const struct rect *rect, struct strrsrc *text_src, int32_t scroll_step,
	int32_t idle_event_id);
void TextListDestroy(struct text_list **ctx_mod);
void text_list_destroy(struct object *self);

void text_list_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds);
void text_list_on_idle_spare(struct evt_handler *self, void *task);
