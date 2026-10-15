#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "view.h"

struct canvas;
struct screen_dc;

struct scroll_view_ops {
	struct view_ops base;
};

struct scroll_view {
	struct view base;
	struct canvas *content;
	struct rect scroll_rect;
	int32_t interval;
	int32_t content_h;
};

#define SCROLL_VIEW_OPS(p) ((const struct scroll_view_ops *) ((const struct object *) (p))->ops)

extern const struct scroll_view_ops SCROLL_VIEW_OPS_TABLE;

void ScrollViewInit(struct scroll_view *self, int32_t id, const struct rect *rect, struct canvas *content,
	const struct rect *content_rect, bool auto_start);
struct scroll_view *ScrollViewCreate(int32_t id, const struct rect *rect, struct canvas *content,
	const struct rect *content_rect, bool auto_start);
void ScrollViewDestroy(struct scroll_view **ctx_mod);
void scroll_view_destroy(struct object *self);

void scroll_view_on_event(struct evt_handler *self, struct cmd_event *event);
void scroll_view_on_idle_spare(struct evt_handler *self, void *task);
void scroll_view_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds);
