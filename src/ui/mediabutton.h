#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../app/event.h"
#include "../base/geom.h"
#include "mediaview.h"

struct cmd_event;
struct ctrl_desc;
struct evt_handler;
struct media_button;

struct media_button_ops {
	struct media_view_ops base;
};

struct media_button {
	struct media_view base;
	struct event spec;
	int32_t notify_cmd;
	int32_t notify_p1;
	int32_t notify_p2;
};

#define MEDIA_BUTTON_OPS(p) ((const struct media_button_ops *) ((const struct object *) (p))->ops)

extern const struct media_button_ops MEDIA_BUTTON_OPS_TABLE;

void MediaButtonInit(struct media_button *self, int32_t id, const struct point *pos, int32_t source_id,
	int32_t bg_id, const struct point *anchor, int32_t fg_id, const struct event *spec_src, int32_t cmd,
	int32_t notify_p1, int32_t notify_p2);
struct media_button *MediaButtonCreate(int32_t id, const struct point *pos, int32_t source_id, int32_t bg_id,
	const struct point *anchor, int32_t fg_id, const struct event *spec_src, int32_t cmd, int32_t notify_p1,
	int32_t notify_p2);
void MediaButtonDestroy(struct media_button **ctx_mod);
void media_button_destroy(struct object *self);

void media_button_on_event(struct evt_handler *self, struct cmd_event *event);

void MediaButtonStart(struct media_button *self, const struct event *spec_src, int32_t cmd, int32_t notify_p1,
	int32_t notify_p2);

struct view *MediaButtonFromTemplate(struct ctrl_desc *desc);
