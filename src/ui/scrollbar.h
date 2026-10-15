#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "control.h"
#include "../app/event.h"
#include "../base/geom.h"

struct drawable;
struct screen_dc;

struct scroll_bar_ops {
	struct control_ops base;
};

struct scroll_bar {
	struct control base;
	struct drawable *drawable;
	int32_t page_step;
	int32_t current_part;
	int32_t pressed_part;
	int32_t track_min;
	int32_t track_max;
	int32_t thumb_base;
	int32_t thumb_pos;
	int32_t drag_offset;
	int32_t thumb_size;
	bool is_vertical;
};

#define SCROLL_BAR_OPS(p) ((const struct scroll_bar_ops *) ((const struct object *) (p))->ops)

extern const struct scroll_bar_ops SCROLL_BAR_OPS_TABLE;

void ScrollBarInit(struct scroll_bar *self, int32_t id, const struct event *event, const struct rect *rect,
	int32_t total, int32_t page_step, int32_t type_id);
struct scroll_bar *ScrollBarCreate(int32_t id, const struct event *event, const struct rect *rect, int32_t total,
	int32_t page_step, int32_t type_id);
void ScrollBarDestroy(struct scroll_bar **ctx_mod);
void scroll_bar_destroy(struct object *self);

void scroll_bar_draw_children(struct view *self, struct screen_dc *dc, const struct rect *invalid_rect);
void scroll_bar_on_event(struct evt_handler *self, struct cmd_event *event);
void scroll_bar_on_idle(struct evt_handler *self, void *task);

void ScrollBarSetMax(struct scroll_bar *self, int32_t new_max);
int32_t ScrollBarHitTest(struct scroll_bar *self, const struct point *pt, int32_t h);
bool ScrollBarValueForPart(struct scroll_bar *self, int32_t part, int32_t *out);
void ScrollBarDrawPart(struct scroll_bar *self, struct screen_dc *dc, int32_t part, bool pressed);
