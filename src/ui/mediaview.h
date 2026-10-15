#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "view.h"

struct canvas;
struct ctrl_desc;
struct drawable;
struct media_view;
struct screen_dc;

struct media_view_ops {
	struct view_ops base;
};

struct media_view {
	struct view base;
	struct point anchor;
	struct drawable *source;
	struct drawable *background;
	struct drawable *foreground;
	struct canvas *composite;
};

#define MEDIA_VIEW_OPS(p) ((const struct media_view_ops *) ((const struct object *) (p))->ops)

extern const struct media_view_ops MEDIA_VIEW_OPS_TABLE;

void MediaViewInit(struct media_view *self, int32_t id, const struct point *pos, int32_t source_id, int32_t bg_id,
	const struct point *anchor_arg, int32_t fg_id);
struct media_view *MediaViewCreate(int32_t id, const struct point *pos, int32_t source_id, int32_t bg_id,
	const struct point *anchor_arg, int32_t fg_id);
void MediaViewDestroy(struct media_view **ctx_mod);
void media_view_destroy(struct object *self);

void media_view_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds);

void MediaViewLoadSource(struct media_view *self, int32_t source_id);
void MediaViewSetBackground(struct media_view *self, int32_t bg_id);

struct view *MediaViewFactory(struct ctrl_desc *desc);
